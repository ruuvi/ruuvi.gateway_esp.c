# IXIT 1-AuthMech: Authentication Mechanisms

IXIT-1-AuthMech lists all authentication mechanisms used in the Device Under Test.

---

## IXIT-1-1: Wi-Fi Configuration Interface

### Description
Unauthenticated access to the local Wi-Fi configuration hotspot (Captive Portal/Web-UI) for 
initial device provisioning and network setup.

### Default Status
Active by Default (Transient Provisioning State).
This interface is active only after factory reset or manufacturing to facilitate initial network
provisioning. It is automatically deactivated once the device is successfully configured or after a
defined timeout period.

### Operational Lifecycle
The unauthenticated Wi-Fi hotspot is subject to strict state transitions to minimize the attack surface:
- **Activation:** Only occurs after a factory reset or upon initial power-on from the factory.
- **Auto-Deactivation (Success):** The hotspot is immediately disabled once the device 
  is successfully configured (connected to Ethernet/Wi-Fi and validates cloud connectivity).
- **Auto-Deactivation (Timeout):** If no configuration activity is detected within 1 hour of
  activation, the hotspot is automatically disabled to prevent persistent open access.

### Authentication Factor
None. 
The interface is open to any user within the physical range of the Gateway's Wi-Fi Access Point.

### Password Generation Mechanism
N/A. No passwords are used for this interface.

### Security Guarantees
- Proximity-based access: Configuration is only accessible when the device is in "Setup Mode" 
  (Hotspot active), requiring physical proximity to the device.
- Integrity: Sensitive configuration data sent to the device is protected via the cryptographic 
  mechanisms described below to prevent tampering during transit.

### Cryptographic Details
- Data Protection (Post-Connection):
  - ECDH (Elliptic-Curve Diffie-Hellman): Used for key agreement between the client browser and the 
    Gateway via `Ruuvi-Ecdh-Pub-Key` headers to establish a secure session without a password.
  - AES-CBC: Once the ECDH shared secret is established, sensitive JSON payloads (e.g., Wi-Fi 
    credentials being saved to the gateway) are encrypted using AES-CBC with a random 16-byte IV.
  - SHA-256: Used to derive the symmetric AES key from the ECDH shared secret and to provide a 
    plaintext hash for integrity verification of the encrypted configuration data.
- Transport: Data is sent as a JSON object containing `{ encrypted, iv, hash }` with the 
  `Ruuvi-Ecdh-Encrypted: true` header.

### Brute Force Prevention Mechanism
N/A.
As no authentication values (passwords/PINs) are required, brute-force attacks against
authentication are not applicable to this interface.

---

## IXIT-1-2: LAN Web-UI (Default Challenge-Response)

### Description
Authenticated access to the Gateway Web-UI via LAN (HTTP Port 80) using the device's default
out-of-the-box security mechanism.

### Default Status
Mandatory Out-of-the-Box State.
This is the factory-default configuration.
Access is blocked until the unique per-device password (derived from DEVICEID) is provided.

### Authentication Factor
Username and Password.
- Username: Fixed (`Admin`).
- Password: Unique per-device default password.

### Generation Security Guarantees
The default password is the 16-character hexadecimal representation of 
the `nRF52 64-bit unique device identifier (DEVICEID)`, formatted as uppercase pairs 
separated by colons (e.g., AA:BB:CC:DD:EE:FF:00:11). 
This provides an entropy of 2^64, ensuring that credentials are unique per unit and mathematically 
resistant to brute-force and class-wide automated attacks.

### Cryptographic Details
#### Authentication mechanism
A custom `x-ruuvi-interactive` challenge-response scheme, similar in structure to HTTP Digest:
- The client receives a realm and challenge from the `WWW-Authenticate` header.
- The client computes `SHA256(challenge:MD5(username:realm:password))`.
- The client sends the username and the resulting SHA-256 value in the JSON payload to `/auth`.

#### Session Security
Uses ECDH for key agreement (public keys are exchanged in `Ruuvi-Ecdh-Pub-Key` headers). 
Subsequent sensitive configuration payloads are encrypted via 
AES-CBC (16-byte random IV) and verified with a SHA-256 integrity hash.
The encrypted data is sent in JSON format as `{ encrypted, iv, hash }` object with the 
`Ruuvi-Ecdh-Encrypted: true` HTTP header.

> Note: Although the gateway supports encrypted requests using ECDH/AES, the web browser UI does not encrypt the authentication request itself.

### Brute Force Prevention Mechanism
Time delays between consecutive attempts to authenticate.

---

## IXIT-1-3: LAN Web-UI (User-Defined HTTP Basic)

### Description
Access to the Gateway Web-UI via LAN (HTTP Port 80) using standard HTTP Basic authentication.

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
Username and Password.
Both are fully user-defined.

### Generation Security Guarantees
Users are responsible for the complexity of the credentials. This mode is a legacy compatibility
feature and must be explicitly enabled via manual configuration file modification; it is not
available for selection in the standard Web-UI.

### Cryptographic Details
**None**. Credentials are Base64 encoded (HTTP Basic authentication).

*Note for Audit*: As this operates over HTTP, credentials are sent in "cleartext" relative to the 
network layer. This mode is provided for specific legacy integrations.

### Brute Force Prevention Mechanism
Not implemented.
Users choosing this mode accept the risk associated with standard HTTP Basic behavior.

---

## IXIT-1-4: LAN Web-UI (User-Defined HTTP Digest)

### Description
Access to the Gateway Web-UI via LAN (HTTP Port 80) using standard HTTP Digest (RFC 7616)
authentication.

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
Username and Password.
Both are fully user-defined.

### Generation Security Guarantees
Users are responsible for the complexity of the credentials. This mode is a legacy compatibility
feature and must be explicitly enabled via manual configuration file modification; it is not
available for selection in the standard Web-UI.

### Cryptographic Details
HTTP Digest (RFC 7616) MD5.
Uses a standard nonce-based challenge-response to prevent plaintext password transmission.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-5: LAN Web-UI (Unauthenticated Mode)

### Description
Open access to the Gateway Web-UI via LAN (HTTP Port 80) without requiring credentials.

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
None.

### Generation Security Guarantees
This mode is disabled by default. 
It must be explicitly enabled by the user through the Web-UI.
By enabling this, the user acknowledges that the local network environment is trusted.

### Cryptographic Details
Not applicable. No authentication-related cryptography is used.

### Brute Force Prevention Mechanism
Not applicable.

---

## IXIT-1-6: LAN Web-UI (Access Disabled)

### Description
Complete restriction of the Web-UI interface over the LAN (HTTP Port 80).

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
N/A. Access is denied.

### Generation Security Guarantees
This is a user-configurable state to minimize the device's attack surface. 
When enabled, the Web-UI service is stopped or blocked at the application layer.

### Cryptographic Details
Not applicable.

### Brute Force Prevention Mechanism
Not applicable.

---

## IXIT-1-7: API Data Access (Bearer Token - Read-Only)

### Description
Machine-to-Machine (M2M) read-only access to the /history API endpoint via HTTP (Port 80) 
using a Bearer token.

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
Bearer token.

### Generation Security Guarantees
The token is generated by the user via the Web-UI. 
The system encourages the use of high-entropy, randomly generated strings.

### Cryptographic Details
Token-based. 
The Gateway validates the token provided in the `Authorization: Bearer <token>` header 
against the stored user-configured token.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-8: API Data Access (Bearer Token - Read/Write)

### Description
Machine-to-Machine (M2M) full access (Read/Write) to the Gateway API via HTTP (Port 80) 
using a Bearer token.

### Default Status
Disabled by Default.
This mode can only be enabled by an authenticated administrator through a manual configuration change.

### Authentication Factor
Bearer token.

### Generation Security Guarantees
Independently configured from the Read-Only token. 
Must be explicitly generated and enabled by the user via the Web-UI.

### Cryptographic Details
Token-based. 
The Gateway validates the token provided in the `Authorization: Bearer <token>` header 
against the stored user-configured token.

### Brute Force Prevention Mechanism
Not implemented.
