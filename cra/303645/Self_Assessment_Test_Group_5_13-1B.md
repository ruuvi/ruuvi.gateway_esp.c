# Test group 5.13-1B: Input Data Validation for Network and Service Interfaces

Provision 5.13-1B — Status: **M**. Related IXIT: `IXIT 15-PhyIntf`, `IXIT 28-LogIntf`,
`IXIT 29-InpVal`.

---

## Test case 5.13-1B-1 (conceptual)

**Purpose**: To conceptually assess whether every network-accessible logical interface in
`IXIT 28-LogIntf` is covered by at least one input validation method in `IXIT 29-InpVal` (`a`), and
whether each validation method effectively validates network data at the receiving end to prevent
system manipulations, processing failures, and security exploits (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Logical Interface Coverage and Network Input Validation Effectiveness

| Network Logical Interface ID (`IXIT 28-LogIntf`)                                                       | Mapped Input Validation Method (`IXIT 29-InpVal`)                | Network Input Validation Mechanics & Protocol Rules                                                                                 | Effectiveness Assessment against System Manipulation & Failures                                                                                     | Unit Verdict |
|:-------------------------------------------------------------------------------------------------------|:-----------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`LogIntf-HTTP-Server`**<br>(Port 80 LAN / Captive Portal)                                            | `InpVal-JSON-Schema-Validation`<br>`InpVal-M2M-Token-Validation` | Parses incoming `POST /ruuvi.json` configuration payloads against strict JSON schema types; validates M2M Bearer tokens.            | **Effective.** Rejects malformed JSON syntax, invalid data types, or bad Bearer tokens with HTTP 400/401, preserving NVS parameters.                |   **PASS**   |
| **`LogIntf-FW-Update-Client`**<br>(HTTPS 443 OTA Flasher)                                              | `InpVal-Firmware-Binary-Signature-Verification`                  | Executes `esp_image_verify` post-download and embedded RSA-3072 signature checks over app/data partitions post-reboot.              | **Effective.** Prevents execution of truncated, corrupted, or tampered firmware images; triggers automated dual-slot partition rollback on failure. |   **PASS**   |
| **`ExtSens-Logical-BLE-Radio-Scanning`**<br>(2.4 GHz Air Interface)                                    | `InpVal-BLE-Advertisement-Filtering`                             | Transceiver link-layer CRC checks, PHY layer rules (`1M`/`2M`/`Coded`), manufacturer filters, and 48-bit MAC Whitelists/Blacklists. | **Effective.** Transceiver systematically drops corrupted air frames, RF noise, and blacklisted MACs before UART handoffs to ESP32 task queues.     |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Telemetry`**<br>**`LogIntf-Cloud-HTTPS-Status`**                                | mbedTLS TLS Record Validation & JSON Response Parser             | Validates server TLS certificates, record frame bounds, and HTTP response headers (`X-Ruuvi-Gateway-Rate`).                         | **Effective.** Protects outbound socket loops against malformed server responses, TLS record corruption, or MitM frame injection.                   |   **PASS**   |
| **`LogIntf-Network-DHCP-Client`**<br>**`LogIntf-Network-DNS-Client`**<br>**`LogIntf-Time-NTP-Client`** | LwIP Native Protocol Stack Packet Validation                     | LwIP stack validates UDP/IP packet length fields, transaction IDs, header flags, and checksums.                                     | **Effective.** Invalid DHCP offers, malformed DNS replies, or corrupted NTP packets are dropped at the LwIP stack level without memory leaks.       |   **PASS**   |

* **Conceptual Assessment Justification**:
  1. **Logical Interface Coverage (Unit a):** 100% of the network-accessible logical interfaces
     cataloged in `IXIT 28-LogIntf` are covered by explicit input validation mechanisms in
     `IXIT 29-InpVal` or native LwIP/mbedTLS protocol validation engines.
  2. **Validation Effectiveness (Unit b):** Network data validation operates across all layers—from
     2.4 GHz RF link-layer CRC/MAC filtering on the nRF52 co-processor, to LwIP/mbedTLS stack frame
     checking, to application-layer RSA-3072 firmware signature checks and JSON schema validation.

* **Verdict**: **PASS**

---

## Test case 5.13-1B-2 (functional)

**Purpose**: To functionally verify that data input validation methods across network interfaces in
`IXIT 29-InpVal` prevent the processing of unexpected or malformed data input (`a`), and that all
physical network interfaces are documented in `IXIT 15-PhyIntf` (`b`).

---

### Test Units Functional Assessment Matrix

#### Test Unit A & B: Functional Network Input Fuzzing, Injection Testing, and Interface Documentation Audit

**Testing Methodology**: The test laboratory executed over-the-air BLE packet corruption injections,
network protocol fuzzing across LwIP UDP/TCP ports (DHCP/DNS/NTP/HTTP), and tampered firmware binary
uploads, while verifying that all physical network interfaces (`PhyIntf-Ethernet`, `PhyIntf-WiFi`,
`PhyIntf-BLE-nRF52`) are documented in `IXIT 15-PhyIntf` according to user manual specifications.

| Network Interface & Target Method                                                   | Functional Fuzzing / Network Injection Action Executed                                                                       | Observed DUT Behavior & Network Security Resilience                                                                                                         | Unit Verdict |
|:------------------------------------------------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`PhyIntf-BLE-nRF52`**<br>(`InpVal-BLE-Advertisement-Filtering`)                   | Transmit corrupted BLE advertisements, invalid packet lengths, bad CRCs, and blacklisted MAC payloads over 2.4 GHz channels. | The nRF52811 transceiver drops corrupted frames at the link layer. UART diagnostic logs confirm zero malformed BLE payloads reach ESP32 memory.             |   **PASS**   |
| **`LogIntf-FW-Update-Client`**<br>(`InpVal-Firmware-Binary-Signature-Verification`) | Trigger OTA download of a tampered firmware binary image (bit-flipped text segment and corrupted RSA signature block).       | `esp_image_verify` fails post-download digest check. Update sequence aborts cleanly; the inactive partition is not flagged for boot and DUT stays secure.   |   **PASS**   |
| **`LogIntf-HTTP-Server`**<br>(`InpVal-JSON-Schema-Validation`)                      | Fuzz HTTP Port 80 with malformed JSON, out-of-bounds REST parameters, oversized headers, and bad M2M Bearer tokens.          | The HTTP server layer drops malformed JSON with HTTP 400, rejects invalid Bearer tokens with HTTP 401, and maintains LwIP socket stability without crashes. |   **PASS**   |
| **LwIP Protocol Stack Interfaces**<br>(DHCP / DNS / NTP UDP Clients)                | Inject malformed UDP packet headers, truncated DNS responses, and out-of-bounds NTP timestamp payloads.                      | The LwIP stack discards invalid UDP frames. System time synchronization and IP leasing loops remain stable without task deadlocks.                          |   **PASS**   |

* **Functional Assessment Justification**:
  1. **Unexpected Data Processing Prevention (Unit a):** Over-the-air BLE packet corruption fuzzing,
     REST API payload fuzzing, and tampered firmware image injections confirm that all network input
     validation mechanisms prevent unexpected data processing. Corrupted air frames are dropped by
     the transceiver, bad REST payloads return HTTP 400/401 errors, and invalid firmware binaries
     are rejected post-download.
  2. **Physical Interface Documentation Completeness (Unit b):** Physical hardware inspection and
     user manual audit confirm that all physical network interfaces (`PhyIntf-Ethernet`,
     `PhyIntf-WiFi`, `PhyIntf-BLE-nRF52`) are completely documented in `IXIT 15-PhyIntf`.

* **Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.13-1B-1 & 5.13-1B-2

| Test Case            | Purpose / Focus                           | Assessment Summary                                                                                                    | Unit Verdict |
|:---------------------|:------------------------------------------|:----------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.13-1B-1 Unit a** | Logical Interface Coverage Check          | 100% of network-accessible logical interfaces in `IXIT 28-LogIntf` are covered by input validation methods.           |   **PASS**   |
| **5.13-1B-1 Unit b** | Validation Effectiveness Evaluation       | RF CRC filtering, mbedTLS record checks, and RSA signature verification effectively validate network data inputs.     |   **PASS**   |
| **5.13-1B-2 Unit a** | Functional Fuzzing & Injection Inspection | BLE air fuzzing, REST API payload fuzzing, and tampered firmware injections verify complete network input validation. |   **PASS**   |
| **5.13-1B-2 Unit b** | Physical Interface Documentation Check    | All physical network interfaces (`PhyIntf-Ethernet`, `WiFi`, `BLE-nRF52`) are documented in `IXIT 15-PhyIntf`.        |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Mandatory Provision 5.13-1B of `ETSI EN 303 645`. All data
transferred over network interfaces or between services is subjected to strict input validation at
the receiving end (`IXIT 29-InpVal`). Over-the-air 2.4 GHz BLE advertisements are filtered at the
link layer by the nRF52811 transceiver (CRC checks, PHY rules, and MAC Whitelist/Blacklist masks),
HTTP REST API payloads are validated against JSON schemas, and downstream firmware updates undergo
cryptographic RSA-3072 signature validation (`esp_image_verify`). Functional network fuzzing and
packet injection confirm that malformed or unexpected network inputs are rejected cleanly without
system manipulations, task deadlocks, or memory corruption exploits.

**Group Verdict**: **PASS**
