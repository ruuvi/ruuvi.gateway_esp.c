# IXIT 1-AuthMech: Authentication Mechanisms

The following declarations map the complete authentication profile of the Ruuvi Gateway (DUT),
specifying cross-references to the target entities, authentication factors, cryptographic
primitives, and integrated brute-force prevention metrics.

---

## **ID**: AuthMech-Hotspot-Provisioning

### Description

Unauthenticated access to the local Wi-Fi configuration hotspot (Captive Portal Web-UI) for initial
device onboarding, radio parameter setup, and network target assignment.

* **Target:** User-to-Machine authentication.
* **Network Interface Proximity:** Directly addressable from a wireless network interface.

### Default Status

Active by Default (Transient Provisioning State). This state is automatically terminated once the
device establishes validation paths to an active upstream Ethernet or Wi-Fi network link, or when a
hardcoded 1-hour idle timeout counter expires.

### Authentication Factor

None. The interface is open to any programming platform inside physical over-the-air radio proximity
bounds during its active window.

### Password Generation Mechanism

N/A (No authentication keys or tokens are active).

### Security Guarantees

Relies on physical proximity boundaries. Ensures that initial credential provisioning requires
immediate local access to the radio envelope of the device casing.

### Cryptographic Details

* **Key Agreement:** Uses an Elliptic Curve Diffie-Hellman (ECDH) key encapsulation mechanism via
  custom `Ruuvi-Ecdh-Pub-Key` HTTP headers to derive an ephemeral shared session secret between the
  client browser and the gateway core without transmitting a password over the unencrypted link.
* **Payload Protection:** Post-handshake transaction blocks containing sensitive configuration JSON
  variables are encrypted via AES-CBC using a random 16-byte Initialization Vector (IV). Key
  derivation and block integrity verification are processed via a SHA-256 hash. Data maps out into a
  payload structure containing `{ encrypted, iv, hash }` flagged with a `Ruuvi-Ecdh-Encrypted: true`
  header modifier.

### Brute Force Prevention

N/A (As no authentication factors are validated, brute-force injection tracking is mathematically
not applicable).

---

## **ID**: AuthMech-LAN-WebUI-Default

### Description

Authenticated administrative access to the gateway local configuration management Web-UI over the
LAN interface using factory out-of-the-box parameters.

* **Target:** User-to-Machine authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Mandatory Out-of-the-Box State.
This is the factory-default configuration.
Access is blocked until the unique per-device password (derived from DEVICEID) is provided.

### Authentication Factor

Username and Password.

* Username: Fixed (`Admin`).
* Password: Hardcoded unique per-device default credential string.

### Password Generation Mechanism

The default factory credential represents the 16-character hexadecimal mapping of the unique 64-bit
hardware $DEVICEID$ extracted from the nRF52811 co-processor's Factory Information Configuration
Registers (FICR), formatted as uppercase pairs separated by colons (e.g.,
`AA:BB:CC:DD:EE:FF:00:11`). This provides a baseline entropy pool of $2^{64}$, establishing
mathematical resilience against dictionary attacks or class-wide automated credential scanning
exploits.

### Security Guarantees

The authentication routine prevents cleartext security parameters from passing over the local area
network. The mechanism confirms that the interacting entity possesses the matching token signature
before exposing gateway parameters.

### Cryptographic Details

* **Authentication Framework:** Implements a custom application-level `x-ruuvi-interactive`
  challenge-response pipeline. The gateway server sends a unique execution realm and random
  cryptographic nonce challenge inside a `WWW-Authenticate` header response. The browser client
  calculates an intermediate digest string via `MD5(username:realm:password)`, then computes a final
  validation token using `SHA256(challenge:MD5_result)` before transmission to the `/auth` endpoint.
* **Session Protection:** Employs subsequent ECDH key negotiation and AES-CBC encryption arrays for
  configuration modification blocks. *Note for Audit:* The initial challenge-response payload itself
  is processed inside the unencrypted JSON payload layout layer of the browser application stack
  rather than native TLS.
  Subsequent sensitive configuration payloads are encrypted via AES-CBC (16-byte random IV) and
  verified with a SHA-256 integrity hash. The encrypted data is sent in JSON format as
  `{ encrypted, iv, hash }` object with the `Ruuvi-Ecdh-Encrypted: true` HTTP header.

### Brute Force Prevention

A fixed ~1-second server-side delay is applied to every POST request (including the login POST to
`/auth`), throttling online password guessing. There is no account-lockout or failed-attempt
counter.

---

## **ID**: AuthMech-LAN-WebUI-User-Defined

### Description

Authenticated administrative access to the gateway local configuration management Web-UI over the
LAN interface using user-customized access parameters. This represents the permanent deployment
operational state.

* **Target:** User-to-Machine authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Active after user configuration. This layer fully replaces `AuthMech-LAN-WebUI-Default` once the
administrator alters the default credential values during the onboarding walkthrough.

### Authentication Factor

Username and Password. Both parameters are completely user-defined.

### Password Generation Mechanism

N/A (The credential values are generated manually by the system administrator).

### Security Guarantees

Prevents the execution of unauthenticated configuration updates across local subnets and mitigates
the risk of default-credential exploits.

### Cryptographic Details

Identical to `AuthMech-LAN-WebUI-Default`. Employs the `x-ruuvi-interactive` challenge-response
pipeline computing `SHA256(challenge:MD5(username:realm:password))` to eliminate plaintext
verification strings on the wire. Subsequent administrative actions utilize the ECDH/AES-CBC
encryption schema context.

### Brute Force Prevention

A fixed ~1-second server-side delay is applied to every POST request (including the login POST to
`/auth`), throttling online password guessing. There is no account-lockout or failed-attempt
counter.

---

## **ID**: AuthMech-LAN-WebUI-Basic

### Description

Legacy authorization fallback using standard HTTP Basic mechanisms to pass administrative
credentials across the LAN.

* **Target:** User-to-Machine authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Disabled by Default. Can only be activated by an authenticated administrator through manual
configuration file modifications.

### Authentication Factor

Username and Password. Both parameters are completely user-defined.

### Password Generation Mechanism

N/A (Parameters are set manually by the system administrator).

### Security Guarantees

Validates entity identity matching prior to executing setup changes. *Note for Audit:* This provides
zero transport-layer confidentiality or integrity guarantees, as it operates over cleartext HTTP.

### Cryptographic Details

Standard HTTP Basic Authentication framework (IETF RFC 9110). Verification attributes are encoded
using flat, reversible Base64 layouts without secondary hashing or salt wrappers.

### Brute Force Prevention

None. Users electing to explicitly activate this legacy integration mode accept the default risks
associated with cleartext HTTP Basic validation structures.

---

## **ID**: AuthMech-LAN-WebUI-Digest

### Description

Legacy compatibility authorization mode utilizing standard HTTP Digest challenge frameworks across
the local subnet.

* **Target:** User-to-Machine authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Disabled by Default. Requires explicit activation via an authorized modification of configuration
file parameters.

### Authentication Factor

Username and Password. Both parameters are completely user-defined.

### Password Generation Mechanism

N/A

### Security Guarantees

Prevents cleartext credential transmission over local wire pathways via standard challenge masking.

### Cryptographic Details

Standard HTTP Digest Authentication protocol layer conforming to RFC 7616 using an MD5-based
cryptographic challenge-response signature generation structure.

### Brute Force Prevention

None implemented at the application tier for this legacy protocol mode.

---

## **ID**: AuthMech-LAN-WebUI-Unauthenticated

### Description

Open, unrestricted entry to the local gateway configuration management dashboard without requiring
any user credential verification steps.

* **Target:** User-to-Machine tracking.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Disabled by Default. Must be manually enabled by an active administrator who explicitly accepts the
structural risks of the deployment environment.

### Authentication Factor

None.

### Password Generation Mechanism

N/A

### Security Guarantees

None. The operational environment assumes a complete local trust model where the entire network
layer is physically isolated from malicious actors.

### Cryptographic Details

N/A (No cryptographic primitives are applied during access validation processing).

### Brute Force Prevention

N/A

---

## **ID**: AuthMech-LAN-WebUI-Disabled

### Description

Complete programmatic restriction and closure of the local web configuration environment service
layer across the network.

* **Target:** N/A (Access is entirely denied).
* **Network Interface Proximity:** Closed at the network routing interface layer.

### Default Status

Disabled by Default. Can be enabled by an administrator to reduce the device's attack surface after
completing deployment configuration.

### Authentication Factor

N/A

### Password Generation Mechanism

N/A

### Security Guarantees

Enforces absolute logical denial-of-access, eliminating the local web interface as a potential
remote exploitation vector.

### Cryptographic Details

N/A

### Brute Force Prevention

N/A

---

## **ID**: AuthMech-M2M-API-Bearer-RO

### Description

Stateless, machine-to-machine read-only access to device state statistics and historical BLE radio
sweeps via the `/history` local API routing node.

* **Target:** Machine-to-Machine (M2M) authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Disabled by Default. Activated explicitly via administrative dashboard selection.

### Authentication Factor

High-Entropy Bearer Token (`lan_auth_api_key`).

### Password Generation Mechanism

The token is automatically generated on the browser client platform during configuration workflow
setup steps using a high-entropy secure pseudo-random sequence passed through a SHA-256 hash engine
and serialized as a Base64 string via the following pipeline:
`crypto.enc.Base64.stringify(crypto.SHA256(crypto.lib.WordArray.random(32)))`
This outputs a strong, unique token signature. The administrator retains the privilege to manually
override or clear this string with any preferred token structure.

### Security Guarantees

Protects internal data caches from unauthenticated collection or systematic tracking by unauthorized
local platforms. *Note for Audit:* Transmitted tokens are passed over cleartext HTTP headers,
relying on local network-level isolation for on-the-wire confidentiality.

### Cryptographic Details

Token-based stateless tracking interface. The device runtime evaluation task reads parameters
presented inside incoming standard `Authorization: Bearer <token>` HTTP request headers, executing a
fast comparison against the string constant stored inside the local `ruuvi.json` profile structure.

### Brute Force Prevention

High-Entropy Token Defense. The token's high cryptographic entropy renders systematic brute-force
guessing or dictionary lookup attacks mathematically impractical and infeasible within the
operational lifetime of the hardware layout.

---

## **ID**: AuthMech-M2M-API-Bearer-RW

### Description

Stateless, machine-to-machine full read/write programmatic access to the local configuration API
nodes to allow automated gateway adjustment by local automation equipment.

* **Target:** Machine-to-Machine (M2M) authentication.
* **Network Interface Proximity:** Directly addressable from a local network interface (HTTP Port
  80).

### Default Status

Disabled by Default. Requires explicit generation and administrative activation via the Web-UI
panel.

### Authentication Factor

High-Entropy Bearer Token (`lan_auth_api_key_rw`).

### Password Generation Mechanism

Identical to `AuthMech-M2M-API-Bearer-RO`. Generated on-demand via a high-entropy 256-bit random
word matrix mapped into an explicit Base64 representation. It is independently configurable and
uniquely isolated from the read-only key variable array.

### Security Guarantees

Restricts low-level system configuration parameters to authorized automation engines, preventing
unauthenticated parameter manipulation.

### Cryptographic Details

Token-based validation framework. Validates the provided incoming cleartext
`Authorization: Bearer <token>` header string against the dedicated write-access parameter value
stored inside flash configuration blocks.

### Brute Force Prevention

High-Entropy Token Defense. Relies on the mathematical infeasibility of scanning a 256-bit random
search space via serial network connection requests to mitigate brute-force exploitation attempts.

---

## Summary Matrix for the Technical File

| Mechanism ID                           | Target Entity | Authentication Factor   | Network Addressable | Brute Force Protection | Transport Layer Security        |
|:---------------------------------------|:--------------|:------------------------|:-------------------:|:-----------------------|:--------------------------------|
| **AuthMech-Hotspot-Provisioning**      | User (Setup)  | None (Open Hotspot)     |   Yes (Wireless)    | N/A                    | Application ECDH Key / AES-CBC  |
| **AuthMech-LAN-WebUI-Default**         | User (Admin)  | Username / Password     |    Yes (Port 80)    | Login Time Delays      | `x-ruuvi-interactive` SHA-256   |
| **AuthMech-LAN-WebUI-User-Defined**    | User (Admin)  | Username / Password     |    Yes (Port 80)    | Login Time Delays      | `x-ruuvi-interactive` SHA-256   |
| **AuthMech-LAN-WebUI-Basic**           | User (Admin)  | Username / Password     |    Yes (Port 80)    | None                   | None (Cleartext Base64 Header)  |
| **AuthMech-LAN-WebUI-Digest**          | User (Admin)  | Username / Password     |    Yes (Port 80)    | None                   | Standard RFC 7616 MD5 Challenge |
| **AuthMech-LAN-WebUI-Unauthenticated** | User          | None                    |    Yes (Port 80)    | N/A                    | None                            |
| **AuthMech-LAN-WebUI-Disabled**        | None          | N/A (Access Denied)     |         No          | N/A                    | N/A                             |
| **AuthMech-M2M-API-Bearer-RO**         | Machine (M2M) | Read-Only Bearer Token  |    Yes (Port 80)    | High-Entropy Token     | None (Cleartext Bearer Header)  |
| **AuthMech-M2M-API-Bearer-RW**         | Machine (M2M) | Read/Write Bearer Token |    Yes (Port 80)    | High-Entropy Token     | None (Cleartext Bearer Header)  |
