# Test group 5.6-2: Minimization of Disclosed Information in Unauthenticated Contexts

Provision 5.6-2 — Status: **M**. Related IXIT: `IXIT 28-LogIntf`.

---

## Test case 5.6-2-1 (conceptual)

**Purpose**: To conceptually assess whether information disclosed without authentication in the
initialized state across all network-accessible logical interfaces (`IXIT 28-LogIntf`) is correctly
classified (`a`), and whether any disclosed security-relevant information is strictly necessary for
the operation of the DUT (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A: Assessment of Information Security Relevance

* **Requirement**: For each network-accessible logical interface in `IXIT 28-LogIntf`, assess
  whether information indicated as "not security-relevant" in "Disclosed Information" is indeed
  non-security-relevant.

| Logical Interface ID (`IXIT 28-LogIntf`) | Network Access Vector    | Declared Disclosed Information (Unauthenticated)                                                                                                                                                            | Security Relevance Assessment                                                                                                                                                                                                                    | Unit Verdict |
|:-----------------------------------------|:-------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`LogIntf-HTTP-Server`**                | Local LAN / Port 80      | Generic HTTP header (`Server: Ruuvi Gateway`). Software patch numbers and framework build revisions are **suppressed**. Unauthenticated REST requests return HTTP 302 redirects to `/#auth`, gating access. | **Low Security Relevance.** Generic platform signatures carry minimal risk. Unauthenticated requests to `/ruuvi.json` or `/history` redirect to `/#auth`, returning `401 Unauthorized` on `/auth` without leaking system metrics or credentials. |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Telemetry`**      | WAN / Port 443           | None (Payloads wrapped in TLS encrypted session).                                                                                                                                                           | **Zero Information Disclosed.** Complete transport encryption.                                                                                                                                                                                   |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Status`**         | WAN / Port 443           | None (Payloads wrapped in TLS encrypted session).                                                                                                                                                           | **Zero Information Disclosed.** Complete transport encryption.                                                                                                                                                                                   |   **PASS**   |
| **`LogIntf-FW-Update-Client`**           | WAN / Port 443           | None (Payloads wrapped in TLS encrypted session).                                                                                                                                                           | **Zero Information Disclosed.** Complete transport encryption.                                                                                                                                                                                   |   **PASS**   |
| **`LogIntf-Network-DHCP-Client`**        | Local LAN / Ports 67, 68 | Standard DHCP Discover/Request frames exposing hardware MAC address and hostname string.                                                                                                                    | **Standard Protocol Information.** MAC address exposure is a mandatory artifact of L2/L3 Ethernet/Wi-Fi protocol operation.                                                                                                                      |   **PASS**   |
| **`LogIntf-Network-DNS-Client`**         | Local LAN / Port 53      | Outgoing UDP lookup requests revealing target domain descriptors.                                                                                                                                           | **Standard Protocol Information.** Essential for resolving telemetry and update server endpoints.                                                                                                                                                |   **PASS**   |
| **`LogIntf-Time-NTP-Client`**            | WAN / UDP Port 123       | Basic NTP packet flags (Version, Transmit Timestamp).                                                                                                                                                       | **Non-Security Relevant.** Wall-clock sync flags contain zero device security context.                                                                                                                                                           |   **PASS**   |

* **Unit A Verdict**: **PASS**

#### Test Unit B: Necessity of Disclosed Security-Relevant Information

* **Requirement**: Assess whether all security-relevant information disclosed without authentication
  is strictly necessary for the operation of the DUT or required for standard protocol compliance.
* **Evaluation**:
  * The DUT suppresses precise software patch numbers, ESP-IDF framework build hashes, and internal
    memory layouts from unauthenticated HTTP response headers, returning strictly
    `Server: Ruuvi Gateway`.
  * Standard protocol disclosures (MAC address in DHCP, domain lookups in DNS) are explicitly
    permitted by ETSI EN 303 645 Clause 5.6.2.0 as necessary protocol operational requirements.
* **Unit B Verdict**: **PASS**

---

## Test case 5.6-2-2 (functional)

**Purpose**: To functionally verify on the DUT that no unannounced or security-relevant
information (such as internal memory pointers, cleartext credentials, or exact component patch
levels) can be observed from network-accessible interfaces without authentication in the initialized
state.

---

### Test Unit A: Functional Assessment of Unauthenticated Disclosed Information

**Testing Methodology**: The test laboratory executed unauthenticated HTTP requests (`GET /`,
`GET /ruuvi.json`, `GET /history`, `HEAD /`), captured network broadcasts (Wireshark packet
sniffing), and inspected header banners and authentication workflows across active network
interfaces in the initialized state.

| Functional Test Scenario              | Target Interface & Endpoint                                | Observed Wire Output & Unauthenticated Behavior                                                                                                               | Unit Verdict |
|:--------------------------------------|:-----------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Unauthenticated Banners**           | `LogIntf-HTTP-Server` (`HEAD /`)                           | Response contains generic `Server: Ruuvi Gateway` header. Specific software version numbers and build hashes are completely absent.                           |   **PASS**   |
| **Unauthenticated REST Redirect**     | `LogIntf-HTTP-Server` (`GET /ruuvi.json` / `GET /history`) | Server returns **HTTP 302 Found** with `Location: http://<hostname>.local/#auth`. Zero configuration parameters, credentials, or sensor arrays are disclosed. |   **PASS**   |
| **Unauthenticated Auth Router Check** | `LogIntf-HTTP-Server` (`GET /auth`)                        | Server returns **HTTP 401 Unauthorized**. Access is blocked until valid encrypted credentials (`{"login":"user","password":"<hash>"}`) are posted.            |   **PASS**   |
| **LAN DHCP Broadcast Inspection**     | `LogIntf-Network-DHCP-Client`                              | Packet capture confirms DHCP frames contain only standard L2 MAC address and hostname strings.                                                                |   **PASS**   |

**Assessment Justification**: Functional network inspection verifies that unauthenticated access
paths on the DUT disclose only the minimal, generic information declared in `IXIT 28-LogIntf`. All
system parameters, historical sensor buffers, and security-relevant credentials are strictly gated
behind redirect and authentication boundaries.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.6-2-1 & 5.6-2-2

| Test Case          | Purpose / Focus                              | Assessment Summary                                                                                                                                           | Unit Verdict |
|:-------------------|:---------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.6-2-1 Unit a** | Assessment of Disclosed Information          | Disclosed information (generic server header, DHCP MACs) is non-security-relevant or standard protocol data.                                                 |   **PASS**   |
| **5.6-2-1 Unit b** | Operational Necessity Evaluation             | Information disclosed in unauthenticated contexts is strictly necessary for basic L2/L3 protocol operation.                                                  |   **PASS**   |
| **5.6-2-2 Unit a** | Functional Unauthenticated Header Inspection | Unauthenticated GETs to `/ruuvi.json`/`/history` return HTTP 302 redirects to `/#auth`, followed by HTTP 401; zero software version numbers or secrets leak. |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.6-2 of `ETSI EN 303 645`. Information
disclosed without authentication across all network-accessible logical interfaces (
`IXIT 28-LogIntf`) is strictly minimized. Unauthenticated HTTP requests return generic server
platform signatures (`Server: Ruuvi Gateway`) without leaking software patch versions, component
build hashes, or configuration parameters. Functional testing confirms that unauthenticated requests
to system endpoints (`/ruuvi.json`, `/history`) are gated via HTTP 302 redirects to `/#auth` (
returning HTTP 401 Unauthorized) and that standard protocol disclosures (e.g., MAC addresses in
DHCP) are limited to necessary operational bounds.

**Group Verdict**: **PASS**
