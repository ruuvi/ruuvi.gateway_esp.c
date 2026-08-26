# Test group 5.5-5: Authentication and Authorization for Security-Relevant Network Configuration Functions

Provision 5.5-5 — Status: **M F (n)**. Related IXIT: `IXIT 1-AuthMech`, `IXIT 13-SoftServ`,
`IXIT 28-LogIntf`.

---

## Test case 5.5-5-1 (conceptual)

**Purpose**: To conceptually assess whether all device functionalities allowing security-relevant
configuration changes via a network interface require authentication and authorization across all
operational states (`a`), adhering to the test units specified in Test Case 5.5-4-1 (`a`–`d`).

---

### Conceptual Assessment Matrix for Configuration-Changing Services

#### Test Unit A: Application of Test Case 5.5-4-1 Across Configurable Services

* **Requirement**: For every service in `IXIT 13-SoftServ` where `Allows Configuration: Yes` and
  accessibility exists via a network interface in any operational state, evaluate the referenced
  authentication and authorization mechanisms.

| Configurable Service ID (`IXIT 13-SoftServ`) | Target Interface & Operational State           | Inbound Authentication Mechanism (`IXIT 1-AuthMech`)              | Discrimination, Protection & Authorization Assessment                                                                                                                                                                                                                                                                                                                                      | Case Verdict |
|:---------------------------------------------|:-----------------------------------------------|:------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| `SoftServ-Local-Management-WebUI`            | Port 80 (Factory Default & Initialized States) | `AuthMech-LAN-WebUI-Default`<br>`AuthMech-LAN-WebUI-User-Defined` | **Factory Default & User Auth Active.** In the factory state, access to security settings is blocked until the factory-unique default password (derived from $DEVICEID$) is authenticated via nonced SHA-256 challenge-responses. Unauthenticated REST requests are gated via HTTP 302 redirects to `/#auth` (HTTP 401). Post-onboarding, user-defined credentials replace default values. |   **PASS**   |
| `SoftServ-Local-Programmatic-API`            | Port 80 (Initialized State)                    | `AuthMech-M2M-API-Bearer-RW`                                      | **Privilege-Separated Authorization.** Modifying device parameters via `POST /ruuvi.json` requires a 256-bit read/write Bearer token (`lan_auth_api_key_rw`). Read-only tokens (`lan_auth_api_key`) are explicitly rejected for write actions with HTTP 403.                                                                                                                               |   **PASS**   |
| `SoftServ-Hotspot-Orchestration`             | Wireless Local AP (Transient Onboarding State) | `AuthMech-Hotspot-Provisioning`                                   | **Physical Proximity Boundary.** Active transiently during initial setup. Configuration payloads are encrypted via ephemeral ECDH/AES-CBC. Hotspot automatically shuts down upon network connection or 1-hour timeout.                                                                                                                                                                     |   **PASS**   |

* **Excluded Protocol Exemption Note**: In accordance with ETSI TS 103 701 Section 5.5.5.1,
  background network infrastructure clients (e.g., DHCP client on UDP Ports 67/68 under
  `SoftServ-Network-Infrastructure-Clients`) are standard network service protocols required for
  basic network operation and are explicitly excluded from this assessment.

**Assessment Justification**: Every device service that permits security-relevant configuration
changes over a network interface requires active authentication and authorization in both factory
default and operational states. Unauthenticated modification of device parameters is strictly
prevented.

**Verdict**: **PASS**

---

## Test case 5.5-5-2 (functional)

**Purpose**: To functionally verify on the DUT that unauthenticated or unauthorized configuration
changes are rejected while authorized changes are processed (`a`), and to functionally verify
through network discovery tools that all active logical network interfaces are fully documented in
`IXIT 28-LogIntf` (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A: Functional Access and Authorization Testing

* **Testing Methodology**: The test laboratory executed unauthorized POST/GET requests, invalid
  credential handshakes, and token role-escalation tests targeting configuration endpoints (
  `/ruuvi.json`, `/auth`) across factory-default and initialized operational states.

| Functional Test Scenario             | Target Service & Endpoint         | Action Executed on DUT                                                                                     | Observed DUT Response & Behavior                                                                                                                                                                                           | Unit Verdict |
|:-------------------------------------|:----------------------------------|:-----------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Factory Default State Protection** | `SoftServ-Local-Management-WebUI` | Send `POST /ruuvi.json` or `GET /ruuvi.json` without authenticating using the factory $DEVICEID$ password. | Server returns HTTP `302 Found` (`Location: http://<hostname>.local/#auth`). Requesting `/auth` returns HTTP `401 Unauthorized`. Configuration changes are blocked until the unique default credential challenge succeeds. |   **PASS**   |
| **M2M Role Boundary Enforcement**    | `SoftServ-Local-Programmatic-API` | Send `POST /ruuvi.json` presenting a Read-Only Bearer token (`lan_auth_api_key`).                          | Server rejects the configuration mutation with HTTP `403 Forbidden`. Flash settings remain untouched.                                                                                                                      |   **PASS**   |
| **Authorized Configuration Change**  | `SoftServ-Local-Programmatic-API` | Send `POST /ruuvi.json` presenting a valid Read/Write Bearer token (`lan_auth_api_key_rw`).                | Server validates token, updates JSON parameters, and commits changes to NVS flash partition.                                                                                                                               |   **PASS**   |

#### Test Unit B: Completeness of Logical Interface Documentation (Network Interface Scanning)

* **Testing Methodology**: The test laboratory conducted comprehensive port scanning (
  `nmap -p 1-65535 -sV -sU`) and traffic capture sweeps across active Ethernet and Wi-Fi network
  interfaces during boot, setup, and operational phases.
* **Evaluation**: Port scanning confirmed that the DUT exposes only standard Port 80 (HTTP server
  handling Web-UI and REST API) and transient setup access points. No undocumented hidden
  administrative ports, diagnostic backdoors, or unauthenticated control listeners were discovered.
  All exposed listening sockets correspond precisely to declarations in `IXIT 28-LogIntf`.
* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.5-5-1 & 5.5-5-2

| Test Case          | Purpose / Focus                            | Assessment Summary                                                                                                                    | Verdict  |
|:-------------------|:-------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------|:--------:|
| **5.5-5-1 Unit a** | Conceptual Auth Check for Security Changes | Security-relevant configuration endpoints require mandatory authentication in both factory default and initialized states.            | **PASS** |
| **5.5-5-2 Unit a** | Functional Authorization Verification      | Functional testing confirms unauthenticated REST requests redirect to `/#auth` (HTTP 401) and read-only token writes return HTTP 403. | **PASS** |
| **5.5-5-2 Unit b** | Logical Interface Discovery Scanning       | Network port scans confirmed zero undocumented listening interfaces or hidden configuration endpoints.                                | **PASS** |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.5-5 of `ETSI EN 303 645`. All
network-accessible functionalities that allow security-relevant changes (
`SoftServ-Local-Management-WebUI`, `SoftServ-Local-Programmatic-API`,
`SoftServ-Hotspot-Orchestration`) enforce active authentication and authorization in both factory
default and initialized states. Unauthenticated configuration requests trigger HTTP 302 redirects to
`/#auth` (HTTP 401), while token-role escalation returns HTTP 403. Functional testing and network
port scanning confirm that unauthorized configuration changes are prevented and that all logical
network interfaces are fully documented in `IXIT 28-LogIntf`.

**Group Verdict**: **PASS**
