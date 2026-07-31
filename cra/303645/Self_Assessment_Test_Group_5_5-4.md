# Test group 5.5-4: Access to Device Functionality via Network Interface Requires Authentication and Authorization

Provision 5.5-4 — Status: **R**. Related IXIT: `IXIT 1-AuthMech`, `IXIT 13-SoftServ`.

---

## Test case 5.5-4-1 (conceptual)

**Purpose**: To conceptually assess whether all device functionalities accessible via a network
interface in the initialized state reference an authentication mechanism (`a`), whether
authentication mechanisms discriminate subjects and reject invalid credentials (`b`), whether
protection mechanisms are resilient against attacks (`c`), and whether the authorization process
grants access strictly to authenticated subjects with proper rights (`d`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Authentication Mechanism Reference Check

* **Requirement**: Check whether every service in `IXIT 13-SoftServ` accessible via a network
  interface in the initialized state references at least one authentication mechanism.
* **Evaluation**:
  * `SoftServ-Local-Management-WebUI` (Active in initialized state over Port 80): References
    `AuthMech-LAN-WebUI-Default` / `AuthMech-LAN-WebUI-User-Defined` (and
    `SecComMech-WebUI-Session`).
  * `SoftServ-Local-Programmatic-API` (Active in initialized state over Port 80): References
    `AuthMech-M2M-API-Bearer-RO` / `AuthMech-M2M-API-Bearer-RW` (and
    `SecComMech-LAN-Bearer-Authentication`).
  * All outbound client services (`SoftServ-Telemetry-Relay-*`, `SoftServ-OTA-Firmware-Updater`,
    `SoftServ-System-Diagnostics-Reporting`) do not expose inbound listening ports and are out of
    scope for inbound network authentication.
* **Verdict**: **PASS**

#### Test Unit B: Discrimination of Subjects & Rejection of Invalid Credentials

* **Requirement**: Assess whether authentication mechanisms discriminate between multiple subjects
  and reject attempts based on invalid identities or credentials.
* **Evaluation**:
  * **Web-UI Administrative Sessions:** Discriminate subjects via username/password pairs (
    `AuthMech-LAN-WebUI-Default` / `User-Defined`). Nonced challenge-response verification evaluates
    `SHA256(challenge:MD5_result)` submitted via `POST /auth`, rejecting invalid passwords or
    tampered challenge tokens with an HTTP 401 response.
  * **M2M Programmatic API:** Discriminates client roles using distinct 256-bit Bearer tokens (
    `lan_auth_api_key` vs `lan_auth_api_key_rw`). Mismatched or invalid tokens are immediately
    rejected.
* **Verdict**: **PASS**

#### Test Unit C: Resilience and Protection of Authentication Mechanisms

* **Requirement**: Assess whether the means protecting the authentication mechanism provide expected
  security guarantees and resist compromise under the Level Basic Attacker Model.
* **Evaluation**:

| Authentication Mechanism (`IXIT 1-AuthMech`)  | Protection Scheme & Cryptographic Details                                                                            | Resilience & Anti-Compromise Assessment                                                                                                                                                                                                           | Unit Verdict |
|:----------------------------------------------|:---------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `AuthMech-LAN-WebUI-Default` / `User-Defined` | Nonced `x-ruuvi-interactive` challenge-response pipeline (`SHA256(challenge:MD5)`); 1-second server-side POST delay. | **Replay & Password Guessing Defense.** Prevents cleartext password transmission over local networks. Server-side delay (~1s) throttles automated online brute-force attempts. Default credentials derive from unique 64-bit hardware $DEVICEID$. |   **PASS**   |
| `AuthMech-M2M-API-Bearer-RO` / `-RW`          | 256-bit high-entropy Base64 tokens (`crypto.lib.WordArray.random(32)`).                                              | **Search Space Defense.** A 256-bit entropy pool renders systematic online token guessing or brute-force scanning mathematically infeasible.                                                                                                      |   **PASS**   |

* **Verdict**: **PASS**

#### Test Unit D: Authorization and Access Control Effectiveness

* **Requirement**: Assess whether the authorization process grants access to subjects with proper
  access rights and denies access to unauthenticated subjects or subjects with inadequate rights.
* **Evaluation**:
  * **Unauthenticated Requests:** Inbound HTTP requests targeting restricted endpoints (
    `/ruuvi.json`, `/history`) without valid authentication parameters are gated via an HTTP
    `302 Found` redirect (`Location: http://<hostname>.local/#auth`). Subsequent unauthenticated
    requests to `/auth` return HTTP `401 Unauthorized`.
  * **Privilege Separation (M2M API):** Clients presenting a read-only token (`lan_auth_api_key`)
    are granted access to read environmental data (`/history`) and configuration snapshots, but are
    explicitly blocked from executing configuration updates via `POST /ruuvi.json`. Configuration
    updates require the read/write token (`lan_auth_api_key_rw`).
* **Verdict**: **PASS**

---

## Test case 5.5-4-2 (functional)

**Purpose**: To functionally verify on the DUT that unauthenticated subjects or subjects with
invalid credentials/inadequate rights are denied access (`a`), authenticated subjects with proper
rights are granted access (`b`), and authentication protection mechanisms function according to IXIT
documentation (`c`).

---

### Test Units Functional Assessment Matrix

| Test Unit / Scenario                              | Action Executed on DUT                                                                                                                                                                                                     | Observed Functional DUT Behavior                                                                                                                                                                                                                                                              | Unit Verdict |
|:--------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unit a: Deny Unauthenticated / Invalid Access** | 1. Send unauthenticated `GET /ruuvi.json`.<br>2. Send unauthenticated `GET /auth`.<br>3. Submit invalid login payload `POST /auth`.<br>4. Send `POST /ruuvi.json` using a Read-Only token (`lan_auth_api_key`).            | 1. Server returns HTTP `302 Found` (`Location: http://<hostname>.local/#auth`).<br>2. Server returns HTTP `401 Unauthorized`.<br>3. Password verification fails; server returns HTTP `401 Unauthorized` after ~1s delay.<br>4. Server rejects configuration update with HTTP `403 Forbidden`. |   **PASS**   |
| **Unit b: Grant Authorized Access**               | 1. Authenticate via Web-UI sending valid `POST /auth` payload (`{"login":"user","password":"<hash>"}`).<br>2. Send `GET /history` using valid Read-Only token.<br>3. Send `POST /ruuvi.json` using valid Read/Write token. | 1. Authentication succeeds; ECDH session established and resource access granted.<br>2. Server returns JSON sensor metrics array.<br>3. Server processes configuration update and saves to NVS.                                                                                               |   **PASS**   |
| **Unit c: Verify Protection Conformance**         | Monitor network traffic during authentication handshakes and API requests.                                                                                                                                                 | Handshake logs confirm `WWW-Authenticate` challenge headers, `Ruuvi-Ecdh-Pub-Key` exchange, encrypted JSON password challenge payloads (`POST /auth`), and AES-CBC encrypted configuration payloads. Passwords are never sent in cleartext.                                                   |   **PASS**   |

**Assessment Justification**: Functional network testing confirms that the DUT strictly enforces
authentication and authorization boundaries. Unauthenticated REST queries are redirected via HTTP
302 to `/#auth` (returning HTTP 401), invalid login attempts or improperly authorized requests are
rejected, valid credentials grant appropriate access, and cryptographic challenge-response and token
verification schemes operate as documented in `IXIT 1-AuthMech`.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-4-1 & 5.5-4-2

| Test Case             | Purpose / Focus                    | Assessment Summary                                                                                                                             | Verdict  |
|:----------------------|:-----------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------|:--------:|
| **5.5-4-1 Unit a**    | Authentication Reference Check     | All network-exposed services in the initialized state reference an active authentication mechanism.                                            | **PASS** |
| **5.5-4-1 Unit b**    | Subject Discrimination & Rejection | Authenticates distinct subjects/roles and reliably rejects invalid credentials or tokens.                                                      | **PASS** |
| **5.5-4-1 Unit c**    | Protection Scheme Resilience       | Nonced challenge-response, high-entropy tokens, and server login delays protect authentication.                                                | **PASS** |
| **5.5-4-1 Unit d**    | Authorization & Access Control     | Enforces strict privilege separation (read-only vs. read/write tokens) and blocks unauthorized actions via HTTP 302/401/403 controls.          | **PASS** |
| **5.5-4-2 Units a-c** | Functional Access Testing          | Functional testing verifies HTTP 302/401 rejection of unauthenticated/invalid requests, HTTP 403 write blocks, and correct handshake behavior. | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.5-4 of `ETSI EN 303 645`. Access to
all network-exposed functionalities in the initialized operational state (
`SoftServ-Local-Management-WebUI`, `SoftServ-Local-Programmatic-API`) requires authentication and
authorization. The platform discriminates subjects, protects authentication handshakes via nonced
SHA-256 challenge-responses (`x-ruuvi-interactive` with encrypted password payloads) and 256-bit
high-entropy Bearer tokens, gates unauthenticated REST requests via HTTP 302 redirects to `/#auth` (
HTTP 401), and enforces strict role-based privilege separation.

**Group Verdict**: **PASS**
