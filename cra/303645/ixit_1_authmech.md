# CRA: 303 645 IXIT-1-AUTHMECH

IXIT-1-AUTHMECH lists all authentication mechanisms used in the Device Under Test.

---

## IXIT-1-1
### Description
User login over HTTP on port 80 to the Wi-Fi configuration hotspot.

### Authentication Factor
Username and password.

### Generation Security Guarantees
The username is fixed to `Admin`.
The default password is unique per device and is generated from the nRF52 64-bit unique device identifier, formatted as uppercase hexadecimal values separated by `:`.

### Cryptographic Details
#### Authentication mechanism
A custom Ruuvi `x-ruuvi-interactive` challenge-response scheme, similar in structure to HTTP Digest:
- The client receives a realm and challenge from the `WWW-Authenticate` header.
- The client computes `MD5(username:realm:password)` and then `SHA256(challenge:MD5(...))`.
- The client sends the username and the resulting SHA-256 value in the JSON payload to `/auth`.

#### Cryptography used for authentication
- **MD5**
  - Used for the inner password hash: `MD5(username:realm:password)`.
- **SHA-256**
  - Used to bind the password hash to the server challenge: `SHA256(challenge:MD5(...))`.
  - Used to derive the AES key from the ECDH shared secret: `SHA256(shared_secret)`.
  - Used to compute a plaintext hash for integrity in encrypted messages.
- **ECDH (Elliptic-Curve Diffie-Hellman)**
  - Used for key agreement between client and server via `Ruuvi-Ecdh-Pub-Key` headers.
- **AES-CBC**
  - Used for symmetric encryption of messages with a random 16-byte IV and a key derived from ECDH via SHA-256.

> Note: Although the gateway supports encrypted requests using ECDH/AES, the web browser UI does not encrypt the authentication request itself.

#### Cryptography used for saving configuration
After an ECDH key exchange during authentication, the client derives a symmetric AES key from the ECDH shared secret using SHA-256. When configuration or other sensitive data is saved, it is serialized to JSON, encrypted using AES-CBC with a random 16-byte IV, and accompanied by a SHA-256 hash of the plaintext for integrity checking.

The resulting `{ encrypted, iv, hash }` object is sent as JSON with the `Ruuvi-Ecdh-Encrypted: true` header. Therefore, configuration save operations are protected using ECDH, AES-CBC, and SHA-256.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-2
### Description
Access to the Gateway UI over HTTP on port 80 using the default authentication mechanism.

### Authentication Factor
Password protected with the default password.

### Generation Security Guarantees
The default password is unique per device and is generated from the nRF52 64-bit unique device identifier, formatted as uppercase hexadecimal values separated by `:`.

### Cryptographic Details
#### Authentication mechanism
A custom Ruuvi `x-ruuvi-interactive` challenge-response scheme, similar in structure to HTTP Digest:
- The client receives a realm and challenge from the `WWW-Authenticate` header.
- The client computes `MD5(username:realm:password)` and then `SHA256(challenge:MD5(...))`.
- The client sends the username and the resulting SHA-256 value in the JSON payload to `/auth`.

#### Cryptography used for authentication
- **MD5**
  - Used for the inner password hash: `MD5(username:realm:password)`.
- **SHA-256**
  - Used to bind the password hash to the server challenge: `SHA256(challenge:MD5(...))`.
  - Used to derive the AES key from the ECDH shared secret: `SHA256(shared_secret)`.
  - Used to compute a plaintext hash for integrity in encrypted messages.
- **ECDH (Elliptic-Curve Diffie-Hellman)**
  - Used for key agreement between client and server via `Ruuvi-Ecdh-Pub-Key` headers.
- **AES-CBC**
  - Used for symmetric encryption of messages with a random 16-byte IV and a key derived from ECDH via SHA-256.

> Note: Although the gateway supports encrypted requests using ECDH/AES, the web browser UI does not encrypt the authentication request itself.

#### Cryptography used for saving configuration
After an ECDH key exchange during authentication, the client derives a symmetric AES key from the ECDH shared secret using SHA-256. When configuration or other sensitive data is saved, it is serialized to JSON, encrypted using AES-CBC with a random 16-byte IV, and accompanied by a SHA-256 hash of the plaintext for integrity checking.

The resulting `{ encrypted, iv, hash }` object is sent as JSON with the `Ruuvi-Ecdh-Encrypted: true` header. Therefore, configuration save operations are protected using ECDH, AES-CBC, and SHA-256.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-3
### Description
Access to the Gateway UI over HTTP on port 80 using Basic authentication.

### Authentication Factor
Username and password configured by the user.

### Generation Security Guarantees
The username and password can be set by the user to any value.
This mode is not directly configurable via the Web UI; the only way to enable it is to manually save the configuration to the device.

### Cryptographic Details
HTTP Basic authentication.

> Note: Over HTTP, Basic authentication does not provide cryptographic protection for the credentials; they are only encoded for transport.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-4
### Description
Access to the Gateway UI over HTTP on port 80 using Digest authentication.

### Authentication Factor
Username and password configured by the user.

### Generation Security Guarantees
The username and password can be set by the user to any value.
This mode is not directly configurable via the Web UI; the only way to enable it is to manually save the configuration to the device.

### Cryptographic Details
HTTP Digest authentication.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-5
### Description
Access to the Gateway UI over HTTP on port 80 without authentication.

### Authentication Factor
No authentication.

### Generation Security Guarantees
Access is allowed to anyone.
The user must manually enable this authentication mechanism in the Web UI.

### Cryptographic Details
Not applicable.

### Brute Force Prevention Mechanism
Not applicable.

---

## IXIT-1-6
### Description
Access to the Gateway UI over HTTP on port 80 is denied.

### Authentication Factor
No authentication.

### Generation Security Guarantees
Access is denied to the user.
The user must manually enable this authentication mechanism in the Web UI.

### Cryptographic Details
Not applicable.

### Brute Force Prevention Mechanism
Not applicable.

---

## IXIT-1-7
### Description
Read-only access to the `/history` API over HTTP on port 80 using Bearer token authentication.

### Authentication Factor
Bearer token.

### Generation Security Guarantees
The user must manually enable this authentication mechanism in the Web UI and configure the token.

### Cryptographic Details
Bearer token authentication.

### Brute Force Prevention Mechanism
Not implemented.

---

## IXIT-1-8
### Description
Read-write access to the Gateway API over HTTP on port 80 using Bearer token authentication.

### Authentication Factor
Bearer token.

### Generation Security Guarantees
The user must manually enable this authentication mechanism in the Web UI and configure the token.

### Cryptographic Details
Bearer token authentication.

### Brute Force Prevention Mechanism
Not implemented.