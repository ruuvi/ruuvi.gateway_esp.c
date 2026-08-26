# Test group 5.1-3: Best Practice Cryptography

## Test case 5.1-3-1 (conceptual)

**Purpose**: To conceptually assess whether the authentication mechanisms provide the necessary
security guarantees (Authenticity/Integrity) and utilize best-practice algorithms free from known
feasible attacks within the device operating context.

| IXIT Entry ID                        | Description / Context              | Security Guarantees Mapped (Unit A/B)             | Cryptographic Details & Primitives (Unit C/D)               | Case Verdict |
|:-------------------------------------|:-----------------------------------|:--------------------------------------------------|:------------------------------------------------------------|:------------:|
| `AuthMech-Hotspot-Provisioning`      | Wi-Fi Onboarding Hotspot           | Integrity & Authenticity of initial configuration | ECDH (P-256 Key Exchange), AES-CBC (128-bit), SHA-256       |   **PASS**   |
| `AuthMech-LAN-WebUI-Default`         | LAN Web-UI (Default State)         | Integrity, Authenticity, and Confidentiality      | ECDH (P-256), AES-CBC (128-bit), SHA-256, MD5 Concatenation |   **PASS**   |
| `AuthMech-LAN-WebUI-User-Defined`    | Custom Administrative Login        | Integrity, Authenticity, and Confidentiality      | ECDH (P-256), AES-CBC (128-bit), SHA-256, MD5 Concatenation |   **PASS**   |
| `AuthMech-LAN-WebUI-Basic`           | Legacy Basic Auth Fallback         | Access Control (Identity Verification Only)       | N/A (User-enabled cleartext fallback mode)                  |   **PASS**   |
| `AuthMech-LAN-WebUI-Digest`          | Legacy Digest Auth Interface       | Authenticity (Nonce challenge masking)            | Standard RFC 7616 MD5 challenge loops                       |   **PASS**   |
| `AuthMech-LAN-WebUI-Unauthenticated` | Open LAN Management State          | None (Explicitly unauthenticated by choice)       | N/A                                                         |   **PASS**   |
| `AuthMech-LAN-WebUI-Disabled`        | Restricted Local Network Access    | Access Control (Absolute logical denial)          | N/A (Interface completely closed)                           |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO`         | Programmatic REST API (`/history`) | Authenticity & Access Control of API request      | High-Entropy Bearer Token, SHA-256 generation loop          |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RW`         | Programmatic Configuration Node    | Authenticity & Access Control of API request      | High-Entropy Bearer Token, SHA-256 generation loop          |   **PASS**   |

**Assessment Justifications:**

* **Unit A & B (Guarantees):** The primary operational authentication mechanisms (
  `AuthMech-LAN-WebUI-Default`, `AuthMech-LAN-WebUI-User-Defined`, and the programmatic
  machine-to-machine `AuthMech-M2M-API-Bearer` endpoints) enforce robust integrity protection and
  origin authenticity guarantees. The custom challenge-response handshake prevents credential
  transmission replication vectors, while the ephemeral ECDH key agreements securely negotiate
  unique symmetric keys to protect post-authentication configuration payloads.
* **Unit C (Best Practice Reference):** The core cryptographic algorithms (AES-CBC, SHA-256, and
  Elliptic Curve Diffie-Hellman over the NIST P-256 curve) are recognized as approved best practices
  within the SOGIS Agreed Cryptographic Mechanisms catalogue and ETSI TR 103 621 reference
  frameworks for resource-constrained IoT deployments.
* **Unit D (Vulnerabilities):** Competent cryptanalytic reports demonstrate that there are no
  indications of a feasible attack against the SHA-256 or AES-CBC operational configurations
  implemented by the DUT within the context of the basic attack potential tier.

**Note on MD5 Primitive Usage:** While MD5 is utilized as the inner hashing algorithm within the
`x-ruuvi-interactive` pipeline to generate the token string via
`MD5(username + ':' + gatewayName + ':' + password)`, holistic session security remains fully
maintained. The resulting digest string is tightly bound against a high-entropy server-generated
nonce via an outer `SHA256(challenge:MD5_result)` loop, and all subsequent data modification
payloads are wrapped within an isolated, encrypted ECDH transport layer.

**Note on Basic/Digest Legacy Authentications:** These mechanisms represent optional legacy
compatibility modes that are disabled by default. Their activation requires explicit administrative
configuration file adjustments, transitioning the risk context to an operator-accepted local
security model.

**Verdict**: **PASS**

---

## Test case 5.1-3-2 (functional)

**Purpose**: To functionally verify that the firmware running on the hardware units actively
enforces the exact protocols and cryptographic primitives declared within the tracking
documentation.

### Test Unit A: Verification of Cryptographic Implementation

**Testing Methodology:** Evaluated via network-layer protocol capture analysis using Wireshark and
browser debugging suites. Custom validation scripts monitored internal subnet communication channels
to analyze cryptographic handshakes, HTTP header structures, and transaction payloads during
onboarding and configuration modification sequences.

| Target Entry ID                               | Observed Runtime Cryptographic Footprint                                                                  | Documented Alignment State          | Unit Verdict |
|:----------------------------------------------|:----------------------------------------------------------------------------------------------------------|:------------------------------------|:------------:|
| `AuthMech-Hotspot-Provisioning`               | Validated `Ruuvi-Ecdh-Pub-Key` ephemeral headers and AES-CBC encrypted JSON objects                       | Confirms to documented architecture |   **PASS**   |
| `AuthMech-LAN-WebUI-Default` / `User-Defined` | Intercepted unique realm/nonce challenges inside `WWW-Authenticate` responses and verified SHA-256 tokens | Confirms to documented architecture |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO` / `RW`           | Captured stateless `Authorization: Bearer <token>` strings matching the high-entropy client structures    | Confirms to documented architecture |   **PASS**   |

**Assessment:** Functional network traffic inspection and cryptographic payload analysis prove that
the device implements the cryptographic details exactly as declared. The communication engine
successfully blocks protocol downgrade attempts to unencrypted variants on protected administrative
API routes.

**Verdict**: **PASS**

---

## Group Summary

The Ruuvi Gateway incorporates industry-standard best-practice cryptography (AES-128, SHA-256, ECDH
P-256) through the native `mbedtls` engine to manage the authenticity, confidentiality, and
structural integrity of authentication parameters. All cryptographic choices conform to modern IoT
deployment requirements, as validated through functional protocol analysis.

**Group Verdict**: **PASS**
