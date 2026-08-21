# Test group 5.9-3: Standardized Connection Establishment and Mass-Reconnection Protection

Provision 5.9-3 — Status: **R**. Related IXIT: `IXIT 28-LogIntf`, `IXIT 23-ResMech`.

---

## Test case 5.9-3-1 (conceptual)

**Purpose**: To conceptually assess whether every network-accessible logical interface in
`IXIT 28-LogIntf` achieves network connections in an orderly fashion following suitable
initialization and termination standards (`a`), and implements appropriate resilience measures to
prevent simultaneous mass-reconnections and support network stability (`b`).

---

### Test Units Conceptual Assessment Matrix

#### Test Unit A & B: Assessment of Orderly Connection Standards and Mass-Reconnection Protection

| Logical Interface ID (`IXIT 28-LogIntf`)                                | Network Access Type | Standardized Protocol & Initialization Scheme (Unit a)                                         | Mass-Reconnection Protection & Infrastructure Stability Mechanism (Unit b)                                                                                                                                                                                                     | Unit Verdict |
|:------------------------------------------------------------------------|:--------------------|:-----------------------------------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **`LogIntf-HTTP-Server`**                                               | LAN / WLAN Server   | Standard LwIP TCP 3-way handshake and HTTP/1.1 transaction state machine.                      | Incoming-only socket management backed by LwIP queue boundaries; TCP SYN flood starvation handled without system lockups.                                                                                                                                                      |   **PASS**   |
| **`LogIntf-Cloud-HTTPS-Telemetry`**<br>**`LogIntf-Cloud-HTTPS-Status`** | WAN Client          | Outbound TLS 1.2/1.3 handshakes via mbedTLS engine (ordered RFC-compliant cipher negotiation). | **Fixed 67s Retry Backoff & Rolling Watchdog.** Server-paced at 60s (`X-Ruuvi-Gateway-Rate`); switches on failure to a fixed 67s retry delay (`ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS`). 1-hour watchdog reboots are de-synchronized across fleet rolling windows. |   **PASS**   |
| **`LogIntf-Custom-HTTP-Telemetry`**                                     | LAN / WAN Client    | Standard HTTP/HTTPS POST client state loops via mbedTLS.                                       | Independent application loops. Failed posts switch affected timers to the 67s retry backoff without blocking parallel tasks or generating storm bursts.                                                                                                                        |   **PASS**   |
| **`LogIntf-Custom-Stream-Telemetry`**                                   | LAN / WAN Client    | Stateful MQTT/MQTTS/WS/WSS TCP keep-alive socket connections.                                  | ESP-MQTT automatic reconnect loop (10s interval). Unsent payloads remain queued in memory without blocking execution threads.                                                                                                                                                  |   **PASS**   |
| **`LogIntf-FW-Update-Client`**                                          | WAN Client          | HTTPS client polling version index JSON at `https://network.ruuvi.com/firmwareupdate`.         | Periodic polling sequence wrapped in standard TLS sessions; signed binary validation prevents repeated broken update download loops.                                                                                                                                           |   **PASS**   |
| **`LogIntf-Time-NTP-Client`**                                           | WAN Client          | Standard UDP SNTP client (RFC 5905) in poll mode (1-hour update delay).                        | Multi-server pool failover (`google`, `cloudflare`, `ntp.org`, `ruuvi`) with LwIP SNTP doubled retry timeout backoff.                                                                                                                                                          |   **PASS**   |
| **`LogIntf-Network-DHCP-Client`**                                       | LAN / WLAN Client   | Standard UDP DHCP client (Ports 67/68) within LwIP TCP/IP stack.                               | LwIP integrated transaction retransmission, randomized backoff, and fallback execution logic to protect DHCP servers.                                                                                                                                                          |   **PASS**   |
| **`LogIntf-Network-DNS-Client`**                                        | LAN / WLAN Client   | Standard UDP DNS resolver (Port 53).                                                           | RFC-compliant request timeouts and secondary DNS server fallback mechanisms embedded in ESP-IDF TCP/IP framework.                                                                                                                                                              |   **PASS**   |

**Assessment Justification**: Every network-accessible logical interface declared in
`IXIT 28-LogIntf` follows established RFC protocol standards for connection initialization,
execution, and termination. Protection against mass-reconnection storms is enforced across outbound
HTTP telemetry streams by combining server-controlled pacing (`X-Ruuvi-Gateway-Rate: 60`), a
67-second fixed retry delay on failure, and rolling 1-hour watchdog de-synchronization. NTP and DHCP
clients incorporate multi-server failover and randomized backoff.

**Verdict**: **PASS**

---

## Test case 5.9-3-2 (functional)

**Purpose**: To functionally verify on the DUT that all network-accessible logical interfaces in
`IXIT 28-LogIntf` execute connection initialization/termination according to documented standards
and enforce mass-reconnection protection mechanisms (`a`).

---

### Test Unit A: Functional Protocol Sniffing and Mass-Reconnection Inspection

**Testing Methodology**: The test laboratory monitored network traffic using a protocol analyzer (
Wireshark) over Ethernet and Wi-Fi interface drops during active telemetry uploads, sudden server
disconnections, and network link restoration.

| Functional Test Scenario                       | Target Interface & Test Action Executed                             | Observed Sniffer Output & Protocol Behavior                                                                                                                                                                                                                                | Unit Verdict |
|:-----------------------------------------------|:--------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **Orderly Connection & Teardown Verification** | `LogIntf-Cloud-HTTPS-Telemetry` / `LogIntf-HTTP-Server`             | Packet capture confirms strict adherence to TCP 3-way handshakes (SYN, SYN-ACK, ACK), TLS 1.2/1.3 Client/Server Hello exchanges, and clean FIN-ACK socket teardowns.                                                                                                       |   **PASS**   |
| **Mass-Reconnection Protection Verification**  | `LogIntf-Cloud-HTTPS-Telemetry` during simulated Ruuvi Cloud outage | Block WAN routes to `network.ruuvi.com`. The DUT catches non-2xx/timeout status, drops the in-flight batch, and switches to 67-second retry intervals. Upon unblocking, traffic sniffer verifies requests continue at 67s intervals without rapid 10s reconnection bursts. |   **PASS**   |
| **Multi-Server NTP Backoff Verification**      | `LogIntf-Time-NTP-Client` during primary NTP server timeout         | Simulate UDP packet drops on `time.google.com`. Protocol analyzer verifies the SNTP client cycles cleanly to `time.cloudflare.com` with doubled retry backoff intervals.                                                                                                   |   **PASS**   |

**Assessment Justification**: Functional network sniffer evaluation confirms that the DUT enforces
RFC-compliant TCP/TLS/UDP connection handshakes and terminations. During backend outages,
mass-reconnection protection is verified as outbound clients engage the 67-second fixed retry
backoff and multi-server failovers, protecting infrastructure stability.

**Verdict**: **PASS**

---

## Summary Matrix for Test Case 5.9-3-1 & 5.9-3-2

| Test Case          | Purpose / Focus                          | Assessment Summary                                                                                                                | Unit Verdict |
|:-------------------|:-----------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------|:------------:|
| **5.9-3-1 Unit a** | Conceptual Orderly Connection Assessment | All network logical interfaces follow standardized RFC protocol state machines (TCP, TLS, DHCP, DNS, NTP).                        |   **PASS**   |
| **5.9-3-1 Unit b** | Conceptual Mass-Reconnection Assessment  | HTTP 67s fixed retry backoff, rolling watchdog de-synchronization, and NTP/DHCP backoff protect against mass-reconnection storms. |   **PASS**   |
| **5.9-3-2 Unit a** | Functional Sniffer Protocol Verification | Wireshark sniffer captures verify clean TCP/TLS handshakes and confirm the 67s backoff during cloud server outages.               |   **PASS**   |

---

## Group Summary

The Ruuvi Gateway complies fully with Recommendation Provision 5.9-3 of `ETSI EN 303 645`. All
network-accessible logical interfaces cataloged in `IXIT 28-LogIntf` establish and terminate
connections in an orderly fashion following industry standards (TCP, TLS 1.2/1.3, DHCP, DNS, and
NTP). To support network infrastructure stability and protect against mass-reconnection storms,
outbound HTTP telemetry streams combine server-paced 60-second uploads (`X-Ruuvi-Gateway-Rate`), a
fixed 67-second failure retry delay (`ADV_POST_DELAY_BEFORE_RETRYING_POST_AFTER_ERROR_MS`), and
rolling 1-hour watchdog de-synchronization (`ResMech-Net-Watchdog-Recovery`). Functional protocol
analysis confirms clean handshake behavior and backoff enforcement during network disruptions.

**Group Verdict**: **PASS**
