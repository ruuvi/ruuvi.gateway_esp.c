# Test group 5.5-1: Communicate Securely Using Best-Practice Cryptography

Provision 5.5-1 — Status: **M**. Related IXIT: `IXIT 11-SecComMech`.

---

## Test case 5.5-1-1 (conceptual)

**Purpose**: To conceptually assess whether the cryptographic methods used across all secure
communication mechanisms in `IXIT 11-SecComMech` provide appropriate security guarantees (`a`),
effectively achieve those guarantees (`b`), align with recognized best-practice reference
catalogues (`c`), and are resilient against feasible cryptanalytic attacks (`d`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Appropriateness of Security Guarantees

* **Requirement**: For each mechanism in `IXIT 11-SecComMech`, assess whether the claimed "Security
  Guarantees" (Confidentiality, Authenticity, Integrity, Access Control) match the protection needs
  of the communication use case.
* **Evaluation**:
  * Outbound cloud telemetry, status, and OTA update tracks require **Confidentiality**,
    **Authenticity**, and **Integrity** to protect against network eavesdropping, MitM tampering,
    and server spoofing.
  * Local administrative Web-UI sessions require **Authenticity**, **Confidentiality**, and
    **Integrity** to protect credentials and configuration changes over local network links.
  * Local Machine-to-Machine (M2M) API endpoints (`SecComMech-LAN-Bearer-Authentication`) require
    **Access Control**, **Authenticity**, and **Privilege Separation** to prevent unauthorized
    command execution and data scraping.
  * Local inter-chip co-processor links (`SecComMech-CoProcessor-SWD-Validation`) require
    **Hardware Code Integrity** and **Anti-Tamper Remediation** to protect the BLE radio scanning
    layer.
* **Verdict**: **PASS**

#### Test Unit B: Mechanism Architecture Appropriateness

* **Requirement**: Assess whether the design of each mechanism effectively achieves its stated
  security guarantees.
* **Evaluation**:
  * `SecComMech-TLS`: Combines server-authenticated TLS 1.2/1.3 handshakes with mbedTLS
    authenticated encryption (AES-GCM) to guarantee socket confidentiality and integrity.
  * `SecComMech-WebUI-Session`: Mitigates local HTTP (Port 80) plaintext risks via
    application-layer hybrid encryption: ECDH key agreement over NIST P-256 (secp256r1), nonced MD5
    password challenge-response, AES-CBC session payload encryption, and SHA-256 message
    authentication.
  * `SecComMech-HMAC-Signing`: Appends 256-bit HMAC-SHA256 headers (`Ruuvi-HMAC-SHA256`) to outbound
    JSON payloads, guaranteeing origin authenticity and payload integrity even across intermediate
    proxies.
  * `SecComMech-LAN-Bearer-Authentication`: Enforces role-segregated, high-entropy 256-bit Bearer
    tokens (`lan_auth_api_key` for read-only `/history` access; `lan_auth_api_key_rw` for full write
    access), rejecting unauthorized M2M requests with HTTP 401 Unauthorized.
  * `SecComMech-CoProcessor-SWD-Validation`: The master ESP32 host halts the nRF52 co-processor over
    internal SWD PCB traces, injects a bare-metal SHA-256 calculation stub into RAM, computes a
    hardware flash digest, and compares it against signed reference blocks, automatically triggering
    firmware restoration on mismatch.
  * `SecComMech-Firmware-Signature-Verification`: Reuses RSA-3072-PSS over SHA-256 to validate main
    and auxiliary data partitions during boot (covered under Test Group 5.3-2).
* **Verdict**: **PASS**

#### Test Unit C: Best Practice Cryptography Reference Catalogue Alignment

* **Requirement**: Evaluate whether all cryptographic primitives, key lengths, and cipher suites
  align with recognized reference catalogues (e.g., SOGIS Agreed Cryptographic Mechanisms, NIST SP
  800-52 Rev. 2, ETSI TR 103 621).

| Communication Mechanism                      | Primitives & Algorithms Implemented                                           | Reference Standard Alignment                                                                                                                           | Evaluated Security Level | Compliance Status |
|:---------------------------------------------|:------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------|:------------------------:|:-----------------:|
| `SecComMech-TLS`                             | TLS 1.2 / TLS 1.3, AES-128/256-GCM, ECDHE (Curve25519/SECP256R1), SHA-256/384 | **NIST SP 800-52 Rev. 2 / SOGIS v1.1:** Modern TLS cipher suites providing forward secrecy and authenticated encryption (AEAD).                        |      $\ge 128$-bit       | **Best Practice** |
| `SecComMech-WebUI-Session`                   | ECDH (NIST P-256), AES-CBC (16-byte IV), SHA-256, Nonced MD5 challenge        | **SOGIS / NIST SP 800-56A:** ECDH P-256 key exchange provides 128-bit security. Nonced password hashing prevents replay attacks over local HTTP links. |         128-bit          | **Best Practice** |
| `SecComMech-HMAC-Signing`                    | HMAC-SHA256 (256-bit symmetric key)                                           | **RFC 2104 / SOGIS v1.1:** Standard message authentication code scheme with strong collision resistance.                                               |         128-bit          | **Best Practice** |
| `SecComMech-LAN-Bearer-Authentication`       | High-entropy 256-bit Bearer Tokens (`lan_auth_api_key` / `_rw`)               | **RFC 6750 / NIST SP 800-63B:** High-entropy pseudo-random tokens generated via `SHA256(WordArray.random(32))`, providing a $2^{256}$ search space.    |         128-bit          | **Best Practice** |
| `SecComMech-CoProcessor-SWD-Validation`      | SWD RAM stub injection, host-driven SHA-256 digest sweep                      | **NIST FIPS 180-4:** Hardware SHA-256 digest validation over internal SWD PCB tracks.                                                                  |         128-bit          | **Best Practice** |
| `SecComMech-Firmware-Signature-Verification` | RSA-3072-PSS with SHA-256                                                     | **SOGIS v1.1 / NIST SP 800-57:** RSA $\ge 3000$ bits with PSS padding provides robust 128-bit signature strength (see Group 5.3-2).                    |         128-bit          | **Best Practice** |

* **Verdict**: **PASS**

#### Test Unit D: Resilience Against Feasible Cryptanalytic Attacks

* **Requirement**: Confirm that used cryptographic primitives have no known vulnerabilities to
  feasible attacks under the Level Basic Attacker Model (Clause D.2).
* **Evaluation**:
  * TLS 1.2/1.3 implementations enforce AEAD ciphers (AES-GCM), eliminating legacy CBC padding
    oracle attacks (e.g., POODLE, BEAST).
  * Web-UI nonced challenge-response hashing prevents replay attacks and credential harvesting over
    unencrypted local Wi-Fi/Ethernet drops.
  * M2M Bearer tokens feature 256-bit cryptographic entropy generated via secure PRNGs, rendering
    brute-force online dictionary attacks mathematically infeasible over serial socket links.
  * Inter-chip SWD communication operates strictly over local, internal PCB copper traces without
    external pin exposure, isolating hardware SHA-256 validation sweeps from network-based
    cryptanalytic attacks.
* **Verdict**: **PASS**

---

## Test case 5.5-1-2 (functional)

**Purpose**: To functionally verify using protocol analysis tools that the DUT executes the
cryptographic mechanisms, parameters, and cipher suites documented in `IXIT 11-SecComMech` during
runtime operation.

---

### Test Unit A: Functional Verification of Protocol Implementation

**Testing Methodology**: The test laboratory captured network traffic dumps (Wireshark) during
system boot, Web-UI administrative sessions, cloud telemetry streaming, and M2M local REST API
queries to analyze negotiated protocol parameters.

| Tested Communication Pathway                                               | Documented Cryptographic Detail (`IXIT 11-SecComMech`)                                         | Observed Functional DUT Behavior                                                                                                                                                                               | Unit Verdict |
|:---------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Outbound Cloud Telemetry & OTA** (`SecComMech-TLS`)                      | TLS 1.2 / TLS 1.3 with AES-128/256-GCM and ECDHE key exchange.                                 | Packet captures confirm TLS client handshakes negotiating `TLS_AES_128_GCM_SHA256` / `ECDHE-RSA-AES128-GCM-SHA256`. Unencrypted fallback is rejected.                                                          |   **PASS**   |
| **Local Web-UI Session** (`SecComMech-WebUI-Session`)                      | ECDH P-256 key agreement (`Ruuvi-Ecdh-Pub-Key`), nonced challenge, AES-CBC payload encryption. | HTTP header analysis confirms `Ruuvi-Ecdh-Pub-Key` exchange during session setup. POST payloads to `/ruuvi.json` are encrypted in AES-CBC with a 16-byte random IV. Cleartext passwords are never transmitted. |   **PASS**   |
| **Outbound Payload Signing** (`SecComMech-HMAC-Signing`)                   | 256-bit HMAC-SHA256 header (`Ruuvi-HMAC-SHA256`).                                              | Outbound HTTP POST payloads carry the 64-character hexadecimal `Ruuvi-HMAC-SHA256` signature header, calculated over the active JSON body.                                                                     |   **PASS**   |
| **Local M2M API** (`SecComMech-LAN-Bearer-Authentication`)                 | HTTP Authorization Bearer token comparison (`lan_auth_api_key` / `lan_auth_api_key_rw`).       | Requests to restricted endpoints (`/history` or `POST /ruuvi.json`) without valid `Authorization: Bearer <token>` headers are immediately rejected with HTTP 401 Unauthorized.                                 |   **PASS**   |
| **Inter-Chip Co-Processor Link** (`SecComMech-CoProcessor-SWD-Validation`) | SWD RAM stub injection and hardware SHA-256 digest validation over internal PCB tracks.        | Oscilloscope and logic analyzer traces on internal SWD lines confirm early-boot RAM stub injection and SHA-256 hash comparison. Injecting bad flash bytes forces automated co-processor code restoration.      |   **PASS**   |

**Assessment Justification**: Functional network traffic analysis and local interface debugging
demonstrate that the DUT enforces all cryptographic settings, protocol versions, and cipher suites
documented in `IXIT 11-SecComMech`. Cleartext transmission of sensitive parameters is prevented
across all active network interfaces.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-1-1 & 5.5-1-2

| Test Case          | Purpose / Focus                     | Assessment Summary                                                                                                                                                        | Verdict  |
|:-------------------|:------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------:|
| **5.5-1-1 Unit a** | Security Guarantees Appropriateness | Claimed guarantees (Confidentiality, Authenticity, Integrity, Access Control, Hardware Integrity) match all communication use cases (cloud, Web-UI, M2M API, inter-chip). | **PASS** |
| **5.5-1-1 Unit b** | Mechanism Design Appropriateness    | TLS 1.2/1.3, hybrid ECDH Web-UI encryption, HMAC signing, 256-bit Bearer tokens, and SWD SHA-256 stubs effectively achieve guarantees.                                    | **PASS** |
| **5.5-1-1 Unit c** | Best Practice Reference Catalogue   | Primitives (AES-GCM, ECDH P-256, SHA-256/384, 256-bit PRNG tokens, RSA-3072-PSS) align with SOGIS, NIST, and RFC standards.                                               | **PASS** |
| **5.5-1-1 Unit d** | Feasible Attack Assessment          | AEAD ciphers, nonced authentication, high-entropy tokens, and internal physical PCB SWD isolation eliminate padding oracle, replay, and brute-force attack vectors.       | **PASS** |
| **5.5-1-2 Unit a** | Functional Traffic Verification     | Packet captures and interface traces confirm runtime enforcement of documented TLS ciphers, ECDH, HMAC signatures, Bearer tokens, and SWD SHA-256 sweeps.                 | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.5-1 of `ETSI EN 303 645`. All secure
communication mechanisms (`IXIT 11-SecComMech`) employ best-practice cryptography (TLS 1.2/1.3 with
AES-GCM, ECDH P-256 key agreement, HMAC-SHA256 payload signing, high-entropy 256-bit Bearer tokens,
and SWD SHA-256 hardware stubs) recognized in SOGIS, NIST, and ETSI reference catalogues. Functional
network protocol analysis and hardware interface auditing confirm that all documented cryptographic
settings are strictly enforced during operation.

**Group Verdict**: **PASS**
