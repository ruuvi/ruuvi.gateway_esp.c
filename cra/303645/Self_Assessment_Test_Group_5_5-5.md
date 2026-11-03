# Test group 5.5-5: Authentication and Authorization for Security-Relevant Network Configuration Functions

Provision 5.5-5 — Status: **M F (n)**. Related IXIT: `IXIT 1-AuthMech`, `IXIT 13-SoftServ`,
`IXIT 28-LogIntf`.

**Evidence status**: Live-DUT tests have been executed, as confirmed by the maintainer. The
corresponding authorization and interface-discovery scripts are being merged separately in
[#1683](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1683) and
[#1684](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1684). Integration and review of the run
evidence in these assessments is deferred to a separate documentation task after those PRs and
[#1685](https://github.com/ruuvi/ruuvi.gateway_esp.c/pull/1685) have merged into `master`.
The mDNS and dynamic SNTP descriptions below are source-derived expectations; this documentation
update asserts no new live-DUT PASS. Existing authorization results are retained from the prior
assessment and are not revalidated here. **PENDING** below means pending evidence integration
and verdict review, not that live-DUT tests have not been executed.

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

* **Assessment Basis**: Firmware source review and the declarations in
  [IXIT 28-LogIntf](ixit_28-LogIntf.md). Scan and traffic-capture results are pending integration;
  the following expectations are not recorded observations from those runs.
* **Source-Derived Expected Behavior**: In the initialized, network-connected state, the DUT is
  expected to expose the HTTP server on TCP Port 80 (Web-UI and REST API) and the mDNS responder
  on UDP Port 5353 (hostname and HTTP service discovery). Transient setup services are limited to
  the provisioning state. The outbound SNTP client uses a local UDP source port dynamically
  assigned from 49152-65535 with a boot-randomized allocator; that port may change after reboot
  or SNTP restart and can appear `open|filtered` in a UDP scan. It is not an inbound NTP service
  (UDP/123 is the server destination). Identifying a scanned UDP port as the SNTP client requires
  correlation with captured outbound traffic to server UDP/123. Source review alone does not
  establish that no undocumented listeners are accessible on the DUT.
* **Verdict**: **PENDING** — integration and review of functional evidence for the revised inventory.

### Follow-Up After Test-Script Merge

After #1683, #1684, and #1685 merge into `master`, the separate documentation task must:

* Update Unit A from the reviewed authorization run evidence associated with #1683.
* Update Unit B from the reviewed scan and packet-capture evidence associated with #1684,
  referencing the merged script revision, tested firmware, run date, and evidence artifacts.
  Record actual Ethernet/Wi-Fi and operating-state coverage, HTTP/mDNS observations, SNTP
  request/source-port correlation, scan limitations, and configuration restoration results.
* Replace expectations with observed results only where supported, then update the unit, summary,
  and group verdicts. Merging scripts alone does not establish a functional PASS.

---

## Summary Matrix for Test Case 5.5-5-1 & 5.5-5-2

| Test Case          | Purpose / Focus                            | Assessment Summary                                                                                                                    |   Verdict   |
|:-------------------|:-------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------|:-----------:|
| **5.5-5-1 Unit a** | Conceptual Auth Check for Security Changes | Security-relevant configuration endpoints require mandatory authentication in both factory default and initialized states.            |  **PASS**   |
| **5.5-5-2 Unit a** | Functional Authorization Verification      | Functional testing confirms unauthenticated REST requests redirect to `/#auth` (HTTP 401) and read-only token writes return HTTP 403. |  **PASS**   |
| **5.5-5-2 Unit b** | Logical Interface Discovery Scanning       | Source-derived HTTP/mDNS and dynamic SNTP expectations documented; scan/capture evidence integration and verdict review pending.      | **PENDING** |

---

## Group Summary

The conceptual assessment and retained authorization results support the authentication and
authorization requirements of Mandatory Provision 5.5-5 of `ETSI EN 303 645`. The assessed
network-accessible functionalities that allow security-relevant changes (
`SoftServ-Local-Management-WebUI`, `SoftServ-Local-Programmatic-API`,
`SoftServ-Hotspot-Orchestration`) enforce active authentication and authorization in both factory
default and initialized states. Unauthenticated configuration requests trigger HTTP 302 redirects to
`/#auth` (HTTP 401), while token-role escalation returns HTTP 403. Completeness of the revised
logical-interface inventory, including mDNS and the dynamic SNTP client socket, remains pending
integration and review of the live-DUT scan and capture evidence in the follow-up documentation task.

**Group Verdict**: **PENDING** — Unit 5.5-5-2 b evidence integration and verdict review outstanding.
