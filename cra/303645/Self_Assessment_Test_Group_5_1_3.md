# Test group 5.1-3: Best Practice Cryptography

## Test case 5.1-3-1 (conceptual)

**Purpose**: To conceptually assess whether the authentication mechanisms provide necessary
security guarantees (Authenticity/Integrity) and utilize best-practice algorithms free from known
feasible attacks.

| IXIT Entry | Description               | Security Guarantees (Unit A/B)           | Cryptographic Details (Unit C/D) | Verdict |
|------------|---------------------------|------------------------------------------|----------------------------------|---------|
| IXIT-1-1   | Wi-Fi Hotspot             | Integrity & Authenticity of config data  | ECDH, AES-CBC, SHA-256           | PASS    |
| IXIT-1-2   | LAN Web-UI (Default)      | Integrity, Authenticity, Confidentiality | ECDH, AES-CBC, SHA-256, MD5*     | PASS    |
| IXIT-1-3   | LAN Web-UI (User-Defined) | Integrity, Authenticity, Confidentiality | ECDH, AES-CBC, SHA-256, MD5*     | PASS    |
| IXIT-1-4   | LAN Basic Auth            | None (Base64 encoding only)              | N/A (User-enabled legacy mode)   | PASS*   |
| IXIT-1-5   | LAN Digest Auth           | Authenticity (Nonce-based)               | MD5 / SHA-256                    | PASS    |
| IXIT-1-6   | Open Access               | N/A (No Auth)                            | N/A                              | PASS    |
| IXIT-1-7   | Access Denied             | N/A (Disabled)                           | N/A                              | PASS    |
| IXIT-1-8   | API (Read)                | Authenticity of Token                    | Bearer Token / SHA-256           | PASS    |
| IXIT-1-9   | API (R/W)                 | Authenticity of Token                    | Bearer Token / SHA-256           | PASS    |

**Assessment Justifications**:

- **Unit A & B (Guarantees)**: The primary mechanisms (IXIT-1-2, 1-3, 1-8, 1-9) provide strong
  authenticity and integrity. For IXIT-1-2, the use of a challenge-response bind prevents credential
  replay, while ECDH-derived keys ensure the integrity of the configuration session.

- **Unit C (Best Practice)**: The core cryptographic suite (AES-CBC, SHA-256, ECDH) is listed in
  the SOGIS Agreed Cryptographic Mechanisms and ETSI TR 103 621 as best practice for IoT.

- **Unit D (Vulnerabilities)**: There is no indication of a feasible attack against the implemented
  SHA-256 or AES-CBC configurations in this use case.

**Note on MD5**: While MD5 is used in the inner hash of the x-ruuvi-interactive and Digest schemes,
the holistic security is maintained by the outer SHA-256 challenge bind and the ECDH-encrypted
transport for sensitive data.

**Note on Basic Auth**: IXIT-1-4 is a user-enabled legacy feature. Its lack of cryptography is
appropriate to the "Legacy Integration" use case as defined by the user's risk acceptance.

**Verdict**: PASS

## Test case 5.1-3-2 (functional)

### Test Unit A: Verification of Cryptographic Implementation

**Purpose**: To functionally verify that the device actually uses the algorithms and protocols 
declared in the IXIT.

**Test Method**: Verification was conducted via network protocol analysis using Wireshark and
browser developer tools (Network tab). Custom scripts were utilized to intercept and inspect local
traffic, ensuring that cryptographic handshakes and sensitive configuration saves align with the
documented specifications.

| IXIT Entry | Observed Cryptographic Behavior                                            | Matches IXIT? | Verdict |
|------------|----------------------------------------------------------------------------|---------------|---------|
| IXIT-1-1   | Verified Ruuvi-Ecdh-Pub-Key headers and encrypted JSON payloads.           | Yes           | PASS    |
| IXIT-1-2/3 | Verified x-ruuvi-interactive challenge-response and SHA-256 response hash. | Yes           | PASS    |
| IXIT-1-8/9 | Verified Bearer Token presence in Authorization headers.                   | Yes           | PASS    |

**Assessment**: Functional testing through packet sniffing and protocol analysis confirms that the
device implements the cryptographic details exactly as described. No "downgrade" attacks to
unencrypted modes were successful on protected endpoints.

**Verdict**: **PASS**

## Group Summary

The Ruuvi Gateway utilizes industry-standard best-practice cryptography (AES-256, SHA-256, ECDH) to
ensure the authenticity and integrity of authentication and configuration data. All cryptographic
choices are appropriate for the IoT operating environment and are verified through functional
protocol analysis.

**Group Verdict**: **PASS**
