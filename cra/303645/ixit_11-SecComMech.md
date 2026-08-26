# IXIT 11-SecComMech: Secure Communication Mechanisms

The following declarations list all communication security mechanisms implemented in the Ruuvi
Gateway (DUT) to guarantee data confidentiality, message authenticity, payload integrity, and
protection against unauthorized replay across active logical domains.

---

## Table C.11: IXIT 11-SecComMech (Secure Communication Mechanisms)

### **ID**: SecComMech-TLS

#### Description

Industry-standard Transport Layer Security (TLS 1.2 and TLS 1.3) protocol implementation driven by
the mbedTLS engine to encrypt transport-layer sockets for cloud endpoints.

#### Interface

* `LogIntf-Cloud-HTTPS-Telemetry`, is remotely accessible via WAN network links.
* `LogIntf-Cloud-HTTPS-Status`, is remotely accessible via WAN network links.
* `LogIntf-FW-Update-Client`, is remotely accessible via WAN network links.
* `LogIntf-Custom-HTTP-Telemetry` (operational strictly when configured for HTTPS destination
  targets), is remotely accessible via network links.
* `LogIntf-Custom-Stream-Telemetry` (operational strictly when configured for MQTTS or WSS secure
  broker profiles), is remotely accessible via network links.

#### Security Guarantees

Guarantees peer server authenticity, cryptographic confidentiality of telemetry datasets, packet
payload integrity, and defense against active interception or Man-in-the-Middle (MitM) replay
injection vectors.

#### Cryptographic Details

Utilizes TLS 1.2 and TLS 1.3 protocols with cipher suites backed by the mbedTLS framework library.
The gateway acts as a client, negotiating cryptographic parameters including curve profiles such as
Curve25519 or SECP256R1 for key exchanges, coupled with symmetric blocks like AES-128-GCM or
AES-256-GCM and SHA-256 or SHA-384 message digests.

---

### **ID**: SecComMech-HMAC-Signing

#### Description

Custom application-layer symmetric message authentication scheme that computes an incremental
cryptographic tag over outbound sensor JSON payloads and telemetry statistics using HMAC-SHA256.

#### Interface

* `LogIntf-Cloud-HTTPS-Telemetry`, is remotely accessible via network links.
* `LogIntf-Cloud-HTTPS-Status`, is remotely accessible via network links.
* `LogIntf-Custom-HTTP-Telemetry`, is remotely accessible via network links.

#### Security Guarantees

Guarantees application-level origin authenticity verification of the DUT, payload integrity
verification (ensuring JSON frames are not altered or truncated in transit), and support for remote
server-driven secret key rotation mechanisms.

#### Cryptographic Details

The mechanism computes a 32-byte (256-bit) message authentication code using HMAC-SHA256 backed by
the generic message-digest framework of mbedTLS. Up to 64-byte separate shared symmetric keys are
isolated in static structures for individual destination profiles. As defined under
`SecParam-HMAC-Symmetric-Secrets`, each key is initialized at startup to the string format of the
unique 64-bit hardware `DEVICEID` extracted from the nRF52 co-processor FICR registry cells (
formatted as uppercase pairs separated by colons, e.g., `AA:BB:CC:DD:EE:FF:00:11`), set via
`hmac_sha256_set_key_for_*()` in `ruuvi_gateway_main.c`:

* `g_hmac_sha256_key_ruuvi` (Ruuvi Cloud endpoint)
* `g_hmac_sha256_key_custom` (Third-party server endpoint)
* `g_hmac_sha256_key_stats` (Status endpoint)

The resulting binary token is hex-encoded into a 64-character string and appended as a custom HTTP
request header: `Ruuvi-HMAC-SHA256`. Keys are dynamically rotatable by the verified receiving
infrastructure via the `Ruuvi-HMAC-KEY` inbound response header.

---

### **ID**: SecComMech-WebUI-Session

#### Description

Multi-layered cryptographic handshake and symmetric payload encryption mechanism designed to secure
the unencrypted local network communication (HTTP Port 80) between web browsers and the device's
internal Web-UI server.

#### Interface

* `LogIntf-HTTP-Server`, is remotely accessible over local subnet interface links.

#### Security Guarantees

Provides local authentication verification via challenge-response hashing (preventing cleartext
password exposure on local networks), key agreement secrecy, post-connection data confidentiality,
and session-level packet payload integrity verification.

#### Cryptographic Details

* **Key Exchange:** Performs an Elliptic Curve Diffie-Hellman (ECDH) key agreement over the NIST
  P-256 (secp256r1) curve across the network using custom HTTP headers (`Ruuvi-Ecdh-Pub-Key`) to
  negotiate an ephemeral session secret without sending raw keys.
* **Key Derivation:** The final session-wide symmetric AES key is derived directly by passing the
  computed ECDH shared secret through a SHA-256 hash.
* **Authentication Challenge:** Uses a custom `x-ruuvi-interactive` scheme where the password
  parameter is never sent over the air. The browser calculates an intermediate token using
  `MD5(username + ':' + gatewayName + ':' + password)` based on the configuration fields in the
  `nvs` partition, then binds it against a unique server-generated nonce challenge via
  `SHA256(challenge:MD5_result)` before transmission to the `/auth` router endpoint.
* **Data Protection:** Post-auth configuration mutations are encrypted using AES in CBC mode. A
  random 16-byte Initialization Vector (IV) is generated for every discrete encryption block using
  secure pseudo-random structures. An appended SHA-256 hash provides block integrity verification.

---

### **ID**: SecComMech-Firmware-Signature-Verification

#### Description

Application-level firmware validation and verification framework that uses a single root of trust
signing key (`SecParam-FW-Verification-Key`) to validate incoming OTA binaries and cross-check
matching partition blocks during system boot routines.

#### Interface

* `LogIntf-FW-Update-Client`, is remotely accessible via network update tracks.
* `LogIntf-USB-Boot-Flasher`, is locally accessible via direct port attachment.

#### Security Guarantees

Guarantees update authenticity, structural partition integrity, protection against compromised or
malformed binary execution, and mitigation against persistent corrupted deployment states via an
automated firmware rollback procedure.

#### Cryptographic Details

Implements an asymmetric validation scheme that reuses the **ESP32 Secure Boot v2 signature format**
(**RSA-3072 with RSA-PSS padding over a SHA-256 digest**). Verification is performed
programmatically by the main application at the application layer because production units utilize a
legacy non-secure secondary bootloader where hardware secure boot eFuses are not burned (see
`IXIT 20-SecBoot`). The scheme anchors protection across three separate runtime components:

1. **Main Application Binary:** Verified explicitly at the application layer during the OTA download
   phase using `esp_image_verify`/`esp_ota_end_patched` APIs.
2. **Web-UI Data Partition (`fatfs_gwui.bin`):** Authenticated during early boot initialization
   using a trailing 4096-byte RSA-3072 signature block embedded within the main application. A
   partition-wide digest is computed via `mbedtls_sha256` and verified against the block using
   `esp_secure_boot_verify_rsa_signature_block`.
3. **Co-processor Code Partition (`fatfs_nrf52.bin`):** Authenticated during early boot execution
   using a trailing 4096-byte RSA-3072 signature block embedded within the main application. A
   partition-wide digest is computed via `mbedtls_sha256` and verified against the block using
   `esp_secure_boot_verify_rsa_signature_block`.

**Rollback Enforcement:** During initial boot verification sequences, if the verification parameters
for either data partition fail validation checks, the application aborts validation, withholds
execution approval markers, and initiates the automated firmware rollback loop to safely restore the
previously verified active slot.

---

### **ID**: SecComMech-LAN-Bearer-Authentication

#### Description

Application-layer Machine-to-Machine (M2M) token-based authentication mechanism that grants
stateless programmatic access to network APIs based on incoming HTTP Authorization header validation
maps.

#### Interface

* `LogIntf-HTTP-Server` (Specifically handling local programmatic requests targeting configuration
  endpoints and `/history` telemetry arrays), is remotely accessible over local subnets.

#### Security Guarantees

Guarantees strict access control, logical segregation of privileges (Privilege Separation), and API
origin authenticity verification. It protects against unauthenticated data scraping or unauthorized
device configuration tampering by third-party network actors.

#### Cryptographic Details

The mechanism processes standard cleartext HTTP headers structured as
`Authorization: Bearer <token>`. The gateway application parses incoming headers and performs a
synchronous string comparison against the tokens (`lan_auth_api_key`, `lan_auth_api_key_rw`) stored
in the `ruuvi.json` manifest within the device `nvs` flash partition space.

Access control boundaries enforce strict cryptographic role segregation:

* **Read-Only Token (`lan_auth_api_key`):** Authorizes client tasks to safely dump general device
  configuration snapshots or read current environmental metrics via the `/history` endpoint loop. It
  blocks any payload containing configuration modification vectors.
* **Read/Write Token (`lan_auth_api_key_rw`):** Grants complete execution privileges, allowing
  automated local network industrial controllers to reconfigure gateway operating attributes
  programmatically via `POST /ruuvi.json`.

If a token is omitted or validation checking fails, the gateway rejects the request with an HTTP
`401 Unauthorized` flag, short-circuiting access before passing execution context down to internal
subsystems.

---

### **ID**: SecComMech-CoProcessor-SWD-Validation

#### Description

A low-level inter-chip hardware-tier security mechanism that uses Serial Wire Debug (SWD) protocol
sequences to validate the code integrity of the peripheral nRF52811 co-processor before functional
metrics processing links are enabled.

#### Interface

* `LogIntf-Internal-SWD-Bus`, is locally isolated on internal PCB traces connecting the ESP32 host
  pins to the target nRF52811 debug port.

#### Security Guarantees

Guarantees hardware code integrity and origin verification for the radio sub-system firmware blocks.
It provides automated boot-time anti-tamper mitigation, ensuring that out-of-band flash
manipulation (e.g., via physical chip programmers) is detected and rectified prior to processing
functional asynchronous serial (UART) communication paths.

#### Cryptographic Details

During early system boot initialization (`nrf52fw_update_fw_step3`), the master ESP32 host halts the
nRF52811 target over the SWD bus framework via `libswd`. The host dynamically injects a specialized
SHA-256 hashing calculation binary stub directly into the co-processor's internal RAM segment (
`NRF52SWD_SHA256_STUB_CODE_ADDR`) and updates the target's Cortex-M execution pointer (PC register).

The injected stub runs a bare-metal hardware sweep across the active nRF52 flash sectors, computing
a full SHA-256 digest (`nrf52swd_calc_sha256_digest_on_nrf52`). The master host reads the calculated
signature block out of RAM (`NRF52SWD_SHA256_STUB_RES_ADDR`) and executes a strict byte comparison
check (`memcmp`) against the reference digest of the signed firmware payload preserved within the
verified `fatfs_nrf52` system flash partition. If a hash mismatch occurs, the host immediately trips
the automated remediation pipeline (`nrf52fw_update_fw_step4`) to overwrite and restore the
co-processor's memory structure before initializing the local BLE parsing tasks.

---

## Summary Matrix for the Technical File

| Mechanism ID                                 | Primary Target Layer       | Cryptographic Primitives                            | Covered Security Guarantees                 |
|:---------------------------------------------|:---------------------------|:----------------------------------------------------|:--------------------------------------------|
| `SecComMech-TLS`                             | Transport / Sockets        | TLS 1.2 / 1.3, AES-GCM, ECDHE, SHA-256/384          | Confidentiality, Authenticity, Integrity    |
| `SecComMech-HMAC-Signing`                    | Application / Payload      | HMAC-SHA256 (mbedTLS generic MD API)                | Authenticity, Integrity                     |
| `SecComMech-WebUI-Session`                   | Session / Local Web UI     | ECDH (P-256), AES-CBC (16-byte IV), SHA-256, MD5    | Authenticity, Confidentiality, Integrity    |
| `SecComMech-Firmware-Signature-Verification` | Application / Boot Loop    | RSA-3072 PSS, SHA-256 (ESP32 Secure Boot v2)        | Authenticity, Integrity, Anti-Replay        |
| `SecComMech-LAN-Bearer-Authentication`       | Application / M2M API      | Cleartext HTTP Bearer Headers (`ruuvi.json` match)  | Access Control, Authenticity                |
| `SecComMech-CoProcessor-SWD-Validation`      | Hardware / Inter-Chip Link | SWD RAM Injection, Host-Driven SHA-256 Verification | Hardware Integrity, Anti-Tamper Remediation |
